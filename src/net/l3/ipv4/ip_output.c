/**
 * @file
 * @brief The Internet Protocol (IP) output module.
 *
 * @date 03.12.09
 * @author Nikolay Korotky
 * @author Vladimir Sokolov
 */

#include <errno.h>

#include <util/math.h>

#include <net/ip.h>
#include <net/udp.h>
#include <net/inet_sock.h>
#include <net/inetdevice.h>
#include <net/route.h>
#include <net/bootp.h>
#include <net/ip_fragment.h>
#include <net/skbuff.h>
#include <net/icmp.h>
#include <net/socket_registry.h>
#include <linux/in.h>
#include <net/netfilter.h>
#include <kernel/printk.h>
#include <net/if_packet.h>
#include <net/if_ether.h>

int rebuild_ip_header(sk_buff_t *skb, unsigned char ttl, unsigned char proto,
		unsigned short id, unsigned short len, in_addr_t saddr,
		in_addr_t daddr/*, ip_options_t *opt*/) {
	static uint16_t global_id = 1230;

	iphdr_t *hdr = skb->nh.iph;
	hdr->version = 4;
	hdr->ihl = IP_MIN_HEADER_SIZE >> 2 /* + opt->optlen */;
	hdr->tos = 0;
	hdr->tot_len = htons(len - ETH_HEADER_SIZE);
	hdr->id = htons(global_id++);
	hdr->frag_off = htons(IP_DF); /* frag_off will be set during fragmentation decision */
	hdr->ttl = 64; /* htons(ttl); */
	hdr->proto = proto;
	hdr->saddr = saddr; /* Changed as in_addr_t is in network-ordered */
	hdr->daddr = daddr;
	ip_send_check(hdr);
	return 0;
}

static void build_ip_packet(struct inet_sock *sk, sk_buff_t *skb) {
	/* IP header has already been built */
	if (sk->sk.opt.so_type == SOCK_RAW)
		return;

	/* We use headers in other way then Linux. So data coinsides with LL header
	 * Ethernet was hardcoded in skb allocations, so be careful
	 */
	skb->nh.raw = skb->mac.raw + ETH_HEADER_SIZE;

	/* Suspicious:
	 *	socket SHOULD NOT set TLL. It's possible, but strange
	 *	socket (!raw || !packet) CAN NOT have information about id. It's not its business.
	 * This functionality belongs to the device. NOT to the socket.
	 * See init_ip_header() usage. It's more correct
	 */
	rebuild_ip_header(skb, sk->uc_ttl, sk->sk.opt.so_protocol, sk->id, skb->len,
			sk->saddr, sk->daddr/*, sk->opt*/);
	return;
}

int ip_queue_send(struct sk_buff *skb) {
	if (0 != nf_test_skb(NF_CHAIN_OUTPUT, NF_TARGET_ACCEPT, skb)) {
		printk("ip_queue_send: skb %p dropped by netfilter\n", skb);
		skb_free(skb);
		return 0;
	}
	return ip_queue_xmit(skb);
}

int ip_queue_xmit(struct sk_buff *skb) {
	skb->protocol = ETH_P_IP;
	return dev_send_skb(skb);
}

/* Fragments skb and sends it to the interface.
 * Returns -1 in case of error
 * As side effect frees incoming skb
 */
static int fragment_skb_and_send(struct sk_buff *skb, struct net_device *dev) {
	struct sk_buff_head *tx_buf = ip_frag(skb, dev->mtu);
	int res = tx_buf ? 0 : -1;
	struct sk_buff *s_tmp;

	skb_free(skb);
	while ((res >= 0) && (s_tmp = skb_queue_pop(tx_buf))) {
		s_tmp->dev = dev;
		res = min(ip_queue_send(s_tmp), res);
	}
	skb_queue_free(tx_buf);
	return res;
}

int ip_send_packet(struct inet_sock *sk, struct sk_buff *skb) {
	int ret;
	in_addr_t dest;
	struct rt_entry *best_route;

	dest = ((sk == NULL) ? skb->nh.iph->daddr : sk->daddr);

	best_route = rt_fib_get_best(dest, NULL);
	if ((best_route == NULL) && (dest != INADDR_BROADCAST)) {
		skb_free(skb);
		return -ENETUNREACH;
	}
	else if ((best_route != NULL) && (sk != NULL)) {
		sk->saddr = inetdev_get_by_dev(best_route->dev)->ifa_address;
	}

	if (sk != NULL) {
		build_ip_packet(sk, skb);
	}

	ret = ip_route(skb, best_route);
	if (ret != 0) {
		skb_free(skb);
		return ret;
	}

	assert(skb->dev != NULL);

	if (skb->len > skb->dev->mtu) {
		if (!(skb->nh.iph->frag_off & htons(IP_DF))) {
			return fragment_skb_and_send(skb, skb->dev);
		} else {
			/* if packet size is greater than MTU and we can't fragment it we can't go further */
			skb_free(skb);
			return -EMSGSIZE;  /* errno? */
		}
	}

	return ip_queue_send(skb);
}

int ip_forward_packet(sk_buff_t *skb) {
	iphdr_t *iph = ip_hdr(skb);
	int optlen = IP_HEADER_SIZE(iph) - IP_MIN_HEADER_SIZE;
	struct rt_entry *best_route = rt_fib_get_best(iph->daddr, NULL);

	/* Drop broadcast and multicast addresses of 2 and 3 layers
	 * Note, that some kinds of those addresses we can't get here, because
	 * they processed in other part of code - see ip_is_local(,true,xxx);
	 * And, of course, loopback packets must not be processed here
	 */
	if ((pkt_type(skb) != PACKET_HOST) || ipv4_is_multicast(iph->daddr)) {
		skb_free(skb);
		return 0;
	}

	/* IP Options is a security violation
	 * Try to return packet as close as possible, so check it before ttl processsing (RFC 1812)
	 */
	if (unlikely(optlen)) {
		icmp_send(skb, ICMP_PARAMETERPROB, 0, htonl(IP_MIN_HEADER_SIZE));
		return -1;
	}

	/* Check TTL and decrease it.
	 * We believe that this skb is ours and we can modify it
	 */
	if (unlikely(iph->ttl <= 1)) {
		icmp_send(skb, ICMP_TIME_EXCEEDED, ICMP_EXC_TTL, 0);
		return -1;
	}
	iph->ttl--; /* All routes have the same length */
	ip_send_check(iph);

	/* Check no route */
	if (!best_route) {
		icmp_send(skb, ICMP_DEST_UNREACH, ICMP_NET_UNREACH, 0);
		return -1;
	}

	/* Should we send ICMP redirect */
	if (skb->dev == best_route->dev) {
		struct sk_buff *s_new = skb_share(skb, SKB_SHARE_NO);
		if (s_new) {
			icmp_send(s_new, ICMP_REDIRECT, (best_route->rt_gateway == INADDR_ANY),
					  best_route->rt_gateway);
		}
		/* We can still proceed here */
	}

	if (ip_route(skb, best_route) < 0) {
		/* So we have something like arp problem */
		if (best_route->rt_gateway == INADDR_ANY) {
			icmp_send(skb, ICMP_DEST_UNREACH, ICMP_HOST_UNREACH, iph->daddr);
		} else {
			icmp_send(skb, ICMP_DEST_UNREACH, ICMP_NET_UNREACH, best_route->rt_gateway);
		}
		return -1;
	}

	/* Fragment packet, if it's required */
	if (skb->len > best_route->dev->mtu) {
		if (!(iph->frag_off & htons(IP_DF))) {
			/* We can perform fragmentation */
			return fragment_skb_and_send(skb, best_route->dev);
		} else {
			/* Fragmentation is disabled */
			icmp_send(skb, ICMP_DEST_UNREACH, ICMP_FRAG_NEEDED,
					  htonl(best_route->dev->mtu << 16)); /* Support RFC 1191 */
			return -1;
		}
	}

	return ip_queue_xmit(skb);
}

void ip_v4_icmp_err_notify(struct sock *sk, int type, int code) {
	so_sk_set_so_error(sk, (type & code << 8));
}
