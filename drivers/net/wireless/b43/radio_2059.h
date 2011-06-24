#ifndef B43_RADIO_2059_H_
#define B43_RADIO_2059_H_

#include <linux/types.h>

#include "phy_ht.h"

/* Values for various registers uploaded on channel switching */
struct b43_phy_ht_channeltab_e_radio2059 {
	/* The channel frequency in MHz */
	u16 freq;
	/* Values for radio registers */
	u8 radio_syn16;
	u8 radio_syn17;
	u8 radio_syn22;
	u8 radio_syn25;
	u8 radio_syn27;
	u8 radio_syn28;
	u8 radio_syn29;
	u8 radio_syn2c;
	u8 radio_syn2d;
	u8 radio_syn37;
	u8 radio_syn41;
	u8 radio_syn43;
	u8 radio_syn47;
	u8 radio_syn4a;
	u8 radio_syn58;
	u8 radio_syn5a;
	u8 radio_syn6a;
	u8 radio_syn6d;
	u8 radio_syn6e;
	u8 radio_syn92;
	u8 radio_syn98;
	u8 radio_rxtx4a;
	u8 radio_rxtx58;
	u8 radio_rxtx5a;
	u8 radio_rxtx6a;
	u8 radio_rxtx6d;
	u8 radio_rxtx6e;
	u8 radio_rxtx92;
	u8 radio_rxtx98;
	/* Values for PHY registers */
	struct b43_phy_ht_channeltab_e_phy phy_regs;
};

const struct b43_phy_ht_channeltab_e_radio2059
*b43_phy_ht_get_channeltab_e_r2059(struct b43_wldev *dev, u16 freq);

#endif /* B43_RADIO_2059_H_ */
