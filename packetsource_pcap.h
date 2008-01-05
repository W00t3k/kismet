/*
    This file is part of Kismet

    Kismet is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Kismet is distributed in the hope that it will be useful,
      but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Kismet; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/*
 * pcapsource handes the largest number of card types.  ideally, everything
 * should be part of pcapsource so that different tools can use them 
 * besides kismet.
 *
 * pcapsource encompasses multiple methods of entering monitor mode and
 * multiple link encapsulation types, the only underlying consistency
 * is the use of libpcap to fetch frames.
 * 
 */

#ifndef __PACKETSOURCE_PCAP_H__
#define __PACKETSOURCE_PCAP_H__

#include "config.h"

#ifdef HAVE_LIBPCAP

#include "packet.h"
#include "packet_ieee80211.h"
#include "packetsource.h"
#include "ifcontrol.h"

extern "C" {
#ifndef HAVE_PCAPPCAP_H
#include <pcap.h>
#else
#include <pcap/pcap.h>
#endif
}

// Include the various variations of BSD radiotap headers from the system if
// we can get them, incidentally pull in other stuff but I'm not sure whats
// needed so we'll leave the extra headers for now
#ifdef HAVE_BSD_SYS_RADIOTAP

#include <sys/socket.h>
#include <net/if.h>
#include <net/if_media.h>

#if defined(SYS_OPENBSD) || defined(SYS_NETBSD)
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <dev/ic/if_wi_ieee.h>

#include <net80211/ieee80211.h>
#include <net80211/ieee80211_ioctl.h>
#include <net80211/ieee80211_radiotap.h>
#endif // Open/Net

#ifdef SYS_FREEBSD
#include <net80211/ieee80211_ioctl.h>
#include <net80211/ieee80211_radiotap.h>
#endif // FreeBSD

#endif // BSD radiotap

// Include the linux system radiotap headers
#ifdef HAVE_LINUX_SYS_RADIOTAP
#include <net/ieee80211_radiotap.h>
#endif

// If we couldn't make any sense of system rt headers (OSX perhaps, or
// win32, or an older linux) then pull in the local radiotap copy
#ifdef HAVE_LOCAL_RADIOTAP
#include "local_ieee80211_radiotap.h"
#endif

// We provide the pcap packet sources
#define USE_PACKETSOURCE_PCAPFILE

// Maximum SSID length for storing
#define MAX_STORED_SSID		32

// for DLT_PRISM_HEADER
#define WLAN_DEVNAMELEN_MAX	16

// Define linktype headers if we don't have them in our includes for some
// reason
#ifndef DLT_PRISM_HEADER
#define DLT_PRISM_HEADER	119
#endif

#ifndef DLT_IEEE802_11_RADIO	
#define DLT_IEEE802_11_RADIO 127
#endif

#ifndef DLT_IEEE802_11_RADIO_AVS
#define DLT_IEEE802_11_RADIO_AVS 163
#endif

// Define kluged local linktype for BSD lame-mode
#define KDLT_BSD802_11		-100

// Extension to radiotap header not yet included in all BSD's
#ifndef IEEE80211_RADIOTAP_F_FCS
#define IEEE80211_RADIOTAP_F_FCS        0x10    /* frame includes FCS */
#endif

#ifndef IEEE80211_IOC_CHANNEL
#define IEEE80211_IOC_CHANNEL 0
#endif

// Prism 802.11 headers from wlan-ng tacked on to the beginning of a
// pcap packet... Snagged from the wlan-ng source
typedef struct {
	uint32_t did;
	uint16_t status;
	uint16_t len;
	uint32_t data;
} __attribute__((__packed__)) p80211item_uint32_t;

typedef struct {
	uint32_t msgcode;
	uint32_t msglen;
	uint8_t devname[WLAN_DEVNAMELEN_MAX];
	p80211item_uint32_t hosttime;
	p80211item_uint32_t mactime;
	p80211item_uint32_t channel;
	p80211item_uint32_t rssi;
	p80211item_uint32_t sq;
	p80211item_uint32_t signal;
	p80211item_uint32_t noise;
	p80211item_uint32_t rate;
	p80211item_uint32_t istx;
	p80211item_uint32_t frmlen;
} __attribute__((__packed__)) wlan_ng_prism2_header;

// wlan-ng (and hopefully others) AVS header, version one.  Fields in
// network byte order.
typedef struct {
	uint32_t version;
	uint32_t length;
	uint64_t mactime;
	uint64_t hosttime;
	uint32_t phytype;
	uint32_t channel;
	uint32_t datarate;
	uint32_t antenna;
	uint32_t priority;
	uint32_t ssi_type;
	int32_t ssi_signal;
	int32_t ssi_noise;
	uint32_t preamble;
	uint32_t encoding;
} avs_80211_1_header;

class PacketSource_Pcap : public KisPacketSource {
public:
	PacketSource_Pcap() {
		fprintf(stderr, "FATAL OOPS:  Packetsource_Pcap() called\n");
		exit(1);
	}

	PacketSource_Pcap(GlobalRegistry *in_globalreg) :
		KisPacketSource(in_globalreg) {
	}

	// No creation or probe for this high-level metasource
	virtual KisPacketSource *CreateSource(GlobalRegistry *in_globalreg, 
										  string in_type, string in_name, 
										  string in_dev) = 0;

	virtual int AutotypeProbe(string in_device) = 0;
	virtual int RegisterSources(Packetsourcetracker *tracker) = 0;

	PacketSource_Pcap(GlobalRegistry *in_globalreg, string in_type, 
					  string in_name, string in_dev) :
		KisPacketSource(in_globalreg, in_type, in_name, in_dev) { 
			// Nothing special here vs. normal
		}
	virtual ~PacketSource_Pcap() { }

	// No management functions at this level
	virtual int EnableMonitor() = 0;
	virtual int DisableMonitor() = 0;
	virtual int FetchChannelCapable() = 0;
	virtual int SetChannel(unsigned int in_ch) = 0;

	// We expect to be drive by the child IPC
	virtual int ChildIPCControl() { return 1; }

	virtual int OpenSource();
	virtual int CloseSource();

	virtual int FetchDescriptor();

	virtual int Poll();

	static void Pcap_Callback(u_char *bp, const struct pcap_pkthdr *header,
							  const u_char *in_data);

	virtual int FetchHardwareChannel();

protected:
	// Mangle linkheaders off a frame, etc
	virtual int ManglePacket(kis_packet *packet);

	// Parse the data link type
    virtual int DatalinkType();

	// Mangle Prism2 and AVS frames
	int Prism2KisPack(kis_packet *packet);
	// If we have radiotap headers, mangle those into kis packets
	int Radiotap2KisPack(kis_packet *packet);
	// If we're just a straight up frame
	int Eight2KisPack(kis_packet *packet);

	pcap_t *pd;
	int datalink_type;
};	

class PacketSource_Pcapfile : public PacketSource_Pcap {
public:
	PacketSource_Pcapfile() {
		fprintf(stderr, "FATAL OOPS:  Packetsource_Pcapfile() called\n");
		exit(1);
	}

	PacketSource_Pcapfile(GlobalRegistry *in_globalreg) :
		PacketSource_Pcap(in_globalreg) {
	}

	// This should return a new object of its own subclass type
	virtual KisPacketSource *CreateSource(GlobalRegistry *in_globalreg, 
										  string in_type, string in_name, 
										  string in_dev) {
		return new PacketSource_Pcapfile(in_globalreg, in_type, in_name, in_dev);
	}

	virtual int AutotypeProbe(string in_device) {
		return 0;
	}

	virtual int RegisterSources(Packetsourcetracker *tracker);

	PacketSource_Pcapfile(GlobalRegistry *in_globalreg, string in_type, 
						  string in_name, string in_dev) :
		PacketSource_Pcap(in_globalreg, in_type, in_name, in_dev) { 
			// Foo
		}
	virtual ~PacketSource_Pcapfile() { }

	virtual int OpenSource();
	virtual int Poll();

	virtual int FetchChannelCapable() { return 0; }
	// Basically do nothing because they have no meaning
	virtual int EnableMonitor() { return 0; }
	virtual int DisableMonitor() { return PACKSOURCE_UNMONITOR_RET_SILENCE; }
	virtual int SetChannel(unsigned int in_ch) { return 0; }
	virtual int HopNextChannel() { return 0; }

protected:
	// Do nothing here, we don't have an independent radio data fetch,
	// we're just filling in the virtual
	virtual void FetchRadioData(kis_packet *in_packet) { };
};

#endif /* have_libpcap */

#endif
