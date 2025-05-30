/**
 *  Copyright (C) 2017 Xilinx, Inc. All rights reserved.
 *  Copyright (c) 2009-2010 Intel Corporation
 *
 *  I2C driver module
 *
 * Author(s):
 * Chien-Wei Lan <chienwei@xilinx.com>
 * Lizhi Hou <lizhih@xilinx.com>
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/io.h>
#include <linux/version.h>

#include "../xocl_drv.h"

#define DRIVER_NAME "xclmgmt-i2c"
/**
 * AXI IIC Bus Interface v2.0
 * http://www.xilinx.com/support/documentation/ip_documentation/axi_iic/v2_0/pg090-axi-iic.pdf
 */

#define	LM63_ADDR			0x4c
/*
 * The LM63 registers
 */

#define LM63_REG_CONFIG1		0x03
#define LM63_REG_CONVRATE		0x04
#define LM63_REG_CONFIG2		0xBF
#define LM63_REG_CONFIG_FAN		0x4A

#define LM63_REG_TACH_COUNT_MSB		0x47
#define LM63_REG_TACH_COUNT_LSB		0x46
#define LM63_REG_TACH_LIMIT_MSB		0x49
#define LM63_REG_TACH_LIMIT_LSB		0x48

#define LM63_REG_PWM_VALUE		0x4C
#define LM63_REG_PWM_FREQ		0x4D
#define LM63_REG_LUT_TEMP_HYST		0x4F
#define LM63_REG_LUT_TEMP(nr)		(0x50 + 2 * (nr))
#define LM63_REG_LUT_PWM(nr)		(0x51 + 2 * (nr))

#define LM63_REG_LOCAL_TEMP		0x00
#define LM63_REG_LOCAL_HIGH		0x05

#define LM63_REG_REMOTE_TEMP_MSB	0x01
#define LM63_REG_REMOTE_TEMP_LSB	0x10
#define LM63_REG_REMOTE_OFFSET_MSB	0x11
#define LM63_REG_REMOTE_OFFSET_LSB	0x12
#define LM63_REG_REMOTE_HIGH_MSB	0x07
#define LM63_REG_REMOTE_HIGH_LSB	0x13
#define LM63_REG_REMOTE_LOW_MSB		0x08
#define LM63_REG_REMOTE_LOW_LSB		0x14
#define LM63_REG_REMOTE_TCRIT		0x19
#define LM63_REG_REMOTE_TCRIT_HYST	0x21

#define LM63_REG_ALERT_STATUS		0x02
#define LM63_REG_ALERT_MASK		0x16

#define LM63_REG_MAN_ID			0xFE
#define LM63_REG_CHIP_ID		0xFF

#define LM96163_REG_TRUTHERM		0x30
#define LM96163_REG_REMOTE_TEMP_U_MSB	0x31
#define LM96163_REG_REMOTE_TEMP_U_LSB	0x32
#define LM96163_REG_CONFIG_ENHANCED	0x45

#define LM63_MAX_CONVRATE		9

#define LM63_MAX_CONVRATE_HZ		32
#define LM96163_MAX_CONVRATE_HZ		26


#define MGMT_XII_MUX_FAN            0x8
#define MGMT_XIIC_MUX_ADDR          0x74

#define LM63_REG_CONFIG1_TCRIT      0x02
#define LM63_REG_CONFIG1_TACH       0x04

#define LM63_REG_CONFIG_FAN_DISABLE 0x00
#define LM63_REG_CONFIG_FAN_RD      0x10
#define LM63_REG_CONFIG_FAN_WR      0x20

#define LM63_REG_PWM_VALUE_FULL     0x3f

#define LM63_REG_REMOTE_TCRIT_64    0x40
#define LM63_REG_REMOTE_TCRIT_32    0x20
#define LM63_REG_REMOTE_TCRIT_16    0x10
#define LM63_REG_REMOTE_TCRIT_8     0x08
#define LM63_REG_REMOTE_TCRIT_4     0x04
#define LM63_REG_REMOTE_TCRIT_2     0x02
#define LM63_REG_REMOTE_TCRIT_1     0x01

#define MAX_TACH_THRESHOLD			0x940
#define MGMT_LM96_IMPLEMENT         0

enum xilinx_i2c_state {
	STATE_DONE,
	STATE_ERROR,
	STATE_START
};

enum xiic_endian {
	LITTLE,
	BIG
};

/**
 * struct xiic_i2c - Internal representation of the XIIC I2C bus
 * @base:	Memory base of the HW registers
 * @wait:	Wait queue for callers
 * @adap:	Kernel adapter representation
 * @tx_msg:	Messages from above to be sent
 * @lock:	Mutual exclusion
 * @tx_pos:	Current pos in TX message
 * @nmsgs:	Number of messages in tx_msg
 * @state:	See STATE_
 * @rx_msg:	Current RX message
 * @rx_pos:	Position within current RX message
 * @endianness: big/little-endian byte order
 */
struct xiic_i2c {
	struct device		*dev;
	void __iomem		*base;
	wait_queue_head_t	wait;
	struct i2c_adapter	adap;
	struct i2c_msg		*tx_msg;
	struct mutex		lock;
	unsigned int		tx_pos;
	unsigned int		nmsgs;
	enum xilinx_i2c_state	state;
	struct i2c_msg		*rx_msg;
	int			rx_pos;
	enum xiic_endian	endianness;
	struct clk *clk;

	uint8_t			last_chan;
	struct i2c_client	*lm63;
};

#define XIIC_MSB_OFFSET 0
#define XIIC_REG_OFFSET (0x100+XIIC_MSB_OFFSET)

/*
 * Register offsets in bytes from RegisterBase. Three is added to the
 * base offset to access LSB (IBM style) of the word
 */
#define XIIC_CR_REG_OFFSET   (0x00+XIIC_REG_OFFSET)	/* Control Register   */
#define XIIC_SR_REG_OFFSET   (0x04+XIIC_REG_OFFSET)	/* Status Register    */
#define XIIC_DTR_REG_OFFSET  (0x08+XIIC_REG_OFFSET)	/* Data Tx Register   */
#define XIIC_DRR_REG_OFFSET  (0x0C+XIIC_REG_OFFSET)	/* Data Rx Register   */
#define XIIC_ADR_REG_OFFSET  (0x10+XIIC_REG_OFFSET)	/* Address Register   */
#define XIIC_TFO_REG_OFFSET  (0x14+XIIC_REG_OFFSET)	/* Tx FIFO Occupancy  */
#define XIIC_RFO_REG_OFFSET  (0x18+XIIC_REG_OFFSET)	/* Rx FIFO Occupancy  */
#define XIIC_TBA_REG_OFFSET  (0x1C+XIIC_REG_OFFSET)	/* 10 Bit Address reg */
#define XIIC_RFD_REG_OFFSET  (0x20+XIIC_REG_OFFSET)	/* Rx FIFO Depth reg  */
#define XIIC_GPO_REG_OFFSET  (0x24+XIIC_REG_OFFSET)	/* Output Register    */

/* Control Register masks */
#define XIIC_CR_ENABLE_DEVICE_MASK        0x01	/* Device enable = 1      */
#define XIIC_CR_TX_FIFO_RESET_MASK        0x02	/* Transmit FIFO reset=1  */
#define XIIC_CR_MSMS_MASK                 0x04	/* Master starts Txing=1  */
#define XIIC_CR_DIR_IS_TX_MASK            0x08	/* Dir of tx. Txing=1     */
#define XIIC_CR_NO_ACK_MASK               0x10	/* Tx Ack. NO ack = 1     */
#define XIIC_CR_REPEATED_START_MASK       0x20	/* Repeated start = 1     */
#define XIIC_CR_GENERAL_CALL_MASK         0x40	/* Gen Call enabled = 1   */

/* Status Register masks */
#define XIIC_SR_GEN_CALL_MASK             0x01	/* 1=a mstr issued a GC   */
#define XIIC_SR_ADDR_AS_SLAVE_MASK        0x02	/* 1=when addr as slave   */
#define XIIC_SR_BUS_BUSY_MASK             0x04	/* 1 = bus is busy        */
#define XIIC_SR_MSTR_RDING_SLAVE_MASK     0x08	/* 1=Dir: mstr <-- slave  */
#define XIIC_SR_TX_FIFO_FULL_MASK         0x10	/* 1 = Tx FIFO full       */
#define XIIC_SR_RX_FIFO_FULL_MASK         0x20	/* 1 = Rx FIFO full       */
#define XIIC_SR_RX_FIFO_EMPTY_MASK        0x40	/* 1 = Rx FIFO empty      */
#define XIIC_SR_TX_FIFO_EMPTY_MASK        0x80	/* 1 = Tx FIFO empty      */

/* Interrupt Status Register masks    Interrupt occurs when...       */
#define XIIC_INTR_ARB_LOST_MASK           0x01	/* 1 = arbitration lost   */
#define XIIC_INTR_TX_ERROR_MASK           0x02	/* 1=Tx error/msg complete */
#define XIIC_INTR_TX_EMPTY_MASK           0x04	/* 1 = Tx FIFO/reg empty  */
#define XIIC_INTR_RX_FULL_MASK            0x08	/* 1=Rx FIFO/reg=OCY level */
#define XIIC_INTR_BNB_MASK                0x10	/* 1 = Bus not busy       */
#define XIIC_INTR_AAS_MASK                0x20	/* 1 = when addr as slave */
#define XIIC_INTR_NAAS_MASK               0x40	/* 1 = not addr as slave  */
#define XIIC_INTR_TX_HALF_MASK            0x80	/* 1 = TX FIFO half empty */

/* The following constants specify the depth of the FIFOs */
#define IIC_RX_FIFO_DEPTH         16	/* Rx fifo capacity               */
#define IIC_TX_FIFO_DEPTH         16	/* Tx fifo capacity               */

/* The following constants specify groups of interrupts that are typically
 * enabled or disables at the same time
 */
#define XIIC_TX_INTERRUPTS                           \
(XIIC_INTR_TX_ERROR_MASK | XIIC_INTR_TX_EMPTY_MASK | XIIC_INTR_TX_HALF_MASK)

#define XIIC_TX_RX_INTERRUPTS (XIIC_INTR_RX_FULL_MASK | XIIC_TX_INTERRUPTS)

/* The following constants are used with the following macros to specify the
 * operation, a read or write operation.
 */
#define XIIC_READ_OPERATION  1
#define XIIC_WRITE_OPERATION 0

/*
 * Tx Fifo upper bit masks.
 */
#define XIIC_TX_DYN_START_MASK            0x0100 /* 1 = Set dynamic start */
#define XIIC_TX_DYN_STOP_MASK             0x0200 /* 1 = Set dynamic stop */

/*
 * The following constants define the register offsets for the Interrupt
 * registers. There are some holes in the memory map for reserved addresses
 * to allow other registers to be added and still match the memory map of the
 * interrupt controller registers
 */
#define XIIC_DGIER_OFFSET    0x1C /* Device Global Interrupt Enable Register */
#define XIIC_IISR_OFFSET     0x20 /* Interrupt Status Register */
#define XIIC_IIER_OFFSET     0x28 /* Interrupt Enable Register */
#define XIIC_RESETR_OFFSET   0x40 /* Reset Register */

#define XIIC_RESET_MASK             0xAUL

#define XIIC_PM_TIMEOUT		1000	/* ms */
/*
 * The following constant is used for the device global interrupt enable
 * register, to enable all interrupts for the device, this is the only bit
 * in the register
 */
#define XIIC_GINTR_ENABLE_MASK      0x80000000UL

#define xiic_tx_space(i2c) ((i2c)->tx_msg->len - (i2c)->tx_pos)
#define xiic_rx_space(i2c) ((i2c)->rx_msg->len - (i2c)->rx_pos)

static void xiic_start_xfer(struct xiic_i2c *i2c);
static void __xiic_start_xfer(struct xiic_i2c *i2c);

/*
 * For the register read and write functions, a little-endian and big-endian
 * version are necessary. Endianness is detected during the probe function.
 * Only the least significant byte [doublet] of the register are ever
 * accessed. This requires an offset of 3 [2] from the base address for
 * big-endian systems.
 */

static inline void xiic_setreg8(struct xiic_i2c *i2c, int reg, u8 value)
{
	if (i2c->endianness == LITTLE)
		iowrite8(value, i2c->base + reg);
	else
		iowrite8(value, i2c->base + reg + 3);
}

static inline u8 xiic_getreg8(struct xiic_i2c *i2c, int reg)
{
	u8 ret;

	if (i2c->endianness == LITTLE)
		ret = ioread8(i2c->base + reg);
	else
		ret = ioread8(i2c->base + reg + 3);
	return ret;
}

static inline void xiic_setreg16(struct xiic_i2c *i2c, int reg, u16 value)
{
	if (i2c->endianness == LITTLE)
		iowrite16(value, i2c->base + reg);
	else
		iowrite16be(value, i2c->base + reg + 2);
}

static inline void xiic_setreg32(struct xiic_i2c *i2c, int reg, int value)
{
	if (i2c->endianness == LITTLE)
		iowrite32(value, i2c->base + reg);
	else
		iowrite32be(value, i2c->base + reg);
}

static inline int xiic_getreg32(struct xiic_i2c *i2c, int reg)
{
	u32 ret;

	if (i2c->endianness == LITTLE)
		ret = ioread32(i2c->base + reg);
	else
		ret = ioread32be(i2c->base + reg);
	return ret;
}

static inline void xiic_irq_dis(struct xiic_i2c *i2c, u32 mask)
{
	u32 ier = xiic_getreg32(i2c, XIIC_IIER_OFFSET);
	xiic_setreg32(i2c, XIIC_IIER_OFFSET, ier & ~mask);
}

static inline void xiic_irq_en(struct xiic_i2c *i2c, u32 mask)
{
	u32 ier = xiic_getreg32(i2c, XIIC_IIER_OFFSET);
	xiic_setreg32(i2c, XIIC_IIER_OFFSET, ier | mask);
}

static inline void xiic_irq_clr(struct xiic_i2c *i2c, u32 mask)
{
	u32 isr = xiic_getreg32(i2c, XIIC_IISR_OFFSET);
	xiic_setreg32(i2c, XIIC_IISR_OFFSET, isr & mask);
}

static inline void xiic_irq_clr_en(struct xiic_i2c *i2c, u32 mask)
{
	xiic_irq_clr(i2c, mask);
	xiic_irq_en(i2c, mask);
}

static void xiic_clear_rx_fifo(struct xiic_i2c *i2c)
{
	u8 sr;
	for (sr = xiic_getreg8(i2c, XIIC_SR_REG_OFFSET);
		!(sr & XIIC_SR_RX_FIFO_EMPTY_MASK);
		sr = xiic_getreg8(i2c, XIIC_SR_REG_OFFSET))
		xiic_getreg8(i2c, XIIC_DRR_REG_OFFSET);
}

static void xiic_reinit(struct xiic_i2c *i2c)
{
	xiic_setreg32(i2c, XIIC_RESETR_OFFSET, XIIC_RESET_MASK);

	msleep(200);
	/* Set receive Fifo depth to maximum (zero based). */
	xiic_setreg8(i2c, XIIC_RFD_REG_OFFSET, IIC_RX_FIFO_DEPTH - 1);

	/* Reset Tx Fifo. */
	xiic_setreg8(i2c, XIIC_CR_REG_OFFSET, XIIC_CR_TX_FIFO_RESET_MASK);

	/* Enable IIC Device, remove Tx Fifo reset & disable general call. */
	xiic_setreg8(i2c, XIIC_CR_REG_OFFSET, XIIC_CR_ENABLE_DEVICE_MASK);

	/* make sure RX fifo is empty */
	xiic_clear_rx_fifo(i2c);

	/* Enable interrupts */
	xiic_setreg32(i2c, XIIC_DGIER_OFFSET, XIIC_GINTR_ENABLE_MASK);

	xiic_irq_clr_en(i2c, XIIC_INTR_ARB_LOST_MASK);
}

static void xiic_deinit(struct xiic_i2c *i2c)
{
	u8 cr;

	xiic_setreg32(i2c, XIIC_RESETR_OFFSET, XIIC_RESET_MASK);

	/* Disable IIC Device. */
	cr = xiic_getreg8(i2c, XIIC_CR_REG_OFFSET);
	xiic_setreg8(i2c, XIIC_CR_REG_OFFSET, cr & ~XIIC_CR_ENABLE_DEVICE_MASK);
}

static void xiic_read_rx(struct xiic_i2c *i2c)
{
	u8 bytes_in_fifo;
	int i;

	bytes_in_fifo = xiic_getreg8(i2c, XIIC_RFO_REG_OFFSET) + 1;

	dev_dbg(i2c->adap.dev.parent,
		"%s entry, bytes in fifo: %d, msg: %d, SR: 0x%x, CR: 0x%x\n",
		__func__, bytes_in_fifo, xiic_rx_space(i2c),
		xiic_getreg8(i2c, XIIC_SR_REG_OFFSET),
		xiic_getreg8(i2c, XIIC_CR_REG_OFFSET));

	if (bytes_in_fifo > xiic_rx_space(i2c))
		bytes_in_fifo = xiic_rx_space(i2c);

	for (i = 0; i < bytes_in_fifo; i++)
		i2c->rx_msg->buf[i2c->rx_pos++] =
			xiic_getreg8(i2c, XIIC_DRR_REG_OFFSET);

	xiic_setreg8(i2c, XIIC_RFD_REG_OFFSET,
		(xiic_rx_space(i2c) > IIC_RX_FIFO_DEPTH) ?
		IIC_RX_FIFO_DEPTH - 1 :  xiic_rx_space(i2c) - 1);
}

static int xiic_tx_fifo_space(struct xiic_i2c *i2c)
{
	/* return the actual space left in the FIFO */
	return IIC_TX_FIFO_DEPTH - xiic_getreg8(i2c, XIIC_TFO_REG_OFFSET) - 1;
}

static void xiic_fill_tx_fifo(struct xiic_i2c *i2c)
{
	u8 fifo_space = xiic_tx_fifo_space(i2c);
	int len = xiic_tx_space(i2c);

	len = (len > fifo_space) ? fifo_space : len;

	dev_dbg(i2c->adap.dev.parent, "%s entry, len: %d, fifo space: %d\n",
		__func__, len, fifo_space);

	while (len--) {
		u16 data = i2c->tx_msg->buf[i2c->tx_pos++];
		if ((xiic_tx_space(i2c) == 0) && (i2c->nmsgs == 1)) {
			/* last message in transfer -> STOP */
			data |= XIIC_TX_DYN_STOP_MASK;
			dev_dbg(i2c->adap.dev.parent, "%s TX STOP\n", __func__);
		}
		xiic_setreg16(i2c, XIIC_DTR_REG_OFFSET, data);
	}
}

static void xiic_wakeup(struct xiic_i2c *i2c, int code)
{
	i2c->tx_msg = NULL;
	i2c->rx_msg = NULL;
	i2c->nmsgs = 0;
	i2c->state = code;
	/* do not wake_up because we do not have interrupt wired */
	/* wake_up(&i2c->wait); */
}

static irqreturn_t xiic_process(int irq, void *dev_id)
{
	struct xiic_i2c *i2c = dev_id;
	u32 pend, isr, ier;
	u32 clr = 0;

	/* Get the interrupt Status from the IPIF. There is no clearing of
 	 * interrupts in the IPIF. Interrupts must be cleared at the source.
 	 * To find which interrupts are pending; AND interrupts pending with
 	 * interrupts masked.
 	 */
	mutex_lock(&i2c->lock);
	isr = xiic_getreg32(i2c, XIIC_IISR_OFFSET);
	ier = xiic_getreg32(i2c, XIIC_IIER_OFFSET);
	pend = isr & ier;
	if (!pend && isr) {
		/* IER can sometime be zero out for no reason */
		pend = isr;
	}

	dev_dbg(i2c->adap.dev.parent, "%s: IER: 0x%x, ISR: 0x%x, pend: 0x%x\n",
		__func__, ier, isr, pend);
	dev_dbg(i2c->adap.dev.parent, "%s: SR: 0x%x, msg: %p, nmsgs: %d\n",
		__func__, xiic_getreg8(i2c, XIIC_SR_REG_OFFSET),
		i2c->tx_msg, i2c->nmsgs);


	/* Service requesting interrupt */
	if ((pend & XIIC_INTR_ARB_LOST_MASK) ||
		((pend & XIIC_INTR_TX_ERROR_MASK) &&
		!(pend & XIIC_INTR_RX_FULL_MASK))) {
		/* bus arbritration lost, or...
		 * Transmit error _OR_ RX completed
 		 * if this happens when RX_FULL is not set
 		 * this is probably a TX error
 		 */

		dev_dbg(i2c->adap.dev.parent, "%s error\n", __func__);

		/* dynamic mode seem to suffer from problems if we just flushes
 		 * fifos and the next message is a TX with len 0 (only addr)
 		 * reset the IP instead of just flush fifos
 		 */
		xiic_reinit(i2c);

		if (i2c->rx_msg)
			xiic_wakeup(i2c, STATE_ERROR);
		if (i2c->tx_msg)
			xiic_wakeup(i2c, STATE_ERROR);
	}
	if (pend & XIIC_INTR_RX_FULL_MASK) {
		/* Receive register/FIFO is full */

		clr |= XIIC_INTR_RX_FULL_MASK;
		if (!i2c->rx_msg) {
			dev_dbg(i2c->adap.dev.parent,
				"%s unexpexted RX IRQ\n", __func__);
			xiic_clear_rx_fifo(i2c);
			goto out;
		}

		xiic_read_rx(i2c);
		if (xiic_rx_space(i2c) == 0) {
			/* this is the last part of the message */
			i2c->rx_msg = NULL;

			/* also clear TX error if there (RX complete) */
			clr |= (isr & XIIC_INTR_TX_ERROR_MASK);

			dev_dbg(i2c->adap.dev.parent,
				"%s end of message, nmsgs: %d\n",
				__func__, i2c->nmsgs);

			/* send next message if this wasn't the last,
 			 * otherwise the transfer will be finialise when
 			 * receiving the bus not busy interrupt
 			 */
			if (i2c->nmsgs > 1) {
				i2c->nmsgs--;
				i2c->tx_msg++;
				dev_dbg(i2c->adap.dev.parent,
					"%s will start next...\n", __func__);

				__xiic_start_xfer(i2c);
			}
		}
	}
	if (pend & XIIC_INTR_BNB_MASK) {
		/* IIC bus has transitioned to not busy */
		clr |= XIIC_INTR_BNB_MASK;

		/* The bus is not busy, disable BusNotBusy interrupt */
		xiic_irq_dis(i2c, XIIC_INTR_BNB_MASK);

		if (!i2c->tx_msg)
			goto out;

		xiic_wakeup(i2c, STATE_DONE);
	}
	if (pend & (XIIC_INTR_TX_EMPTY_MASK | XIIC_INTR_TX_HALF_MASK)) {
		/* Transmit register/FIFO is empty or ½ empty */

		clr |= (pend &
			(XIIC_INTR_TX_EMPTY_MASK | XIIC_INTR_TX_HALF_MASK));

		if (!i2c->tx_msg) {
			dev_dbg(i2c->adap.dev.parent,
				"%s unexpexted TX IRQ\n", __func__);
			goto out;
		}

		xiic_fill_tx_fifo(i2c);

		/* current message sent and there is space in the fifo */
		if (!xiic_tx_space(i2c) && xiic_tx_fifo_space(i2c) >= 2) {
			dev_dbg(i2c->adap.dev.parent,
				"%s end of message sent, nmsgs: %d\n",
				__func__, i2c->nmsgs);
			if (i2c->nmsgs > 1) {
				i2c->nmsgs--;
				i2c->tx_msg++;
				__xiic_start_xfer(i2c);
			} else {
				xiic_irq_dis(i2c, XIIC_INTR_TX_HALF_MASK);

				dev_dbg(i2c->adap.dev.parent,
					"%s Got TX IRQ but no more to do...\n",
					__func__);
			}
		} else if (!xiic_tx_space(i2c) && (i2c->nmsgs == 1))
			/* current frame is sent and is last,
 			 * make sure to disable tx half
 			 */
			xiic_irq_dis(i2c, XIIC_INTR_TX_HALF_MASK);
	}
out:
	dev_dbg(i2c->adap.dev.parent, "%s clr: 0x%x\n", __func__, clr);

	xiic_setreg32(i2c, XIIC_IISR_OFFSET, clr);
	mutex_unlock(&i2c->lock);
	return IRQ_HANDLED;
}

static int xiic_bus_busy(struct xiic_i2c *i2c)
{
	u8 sr = xiic_getreg8(i2c, XIIC_SR_REG_OFFSET);

	return (sr & XIIC_SR_BUS_BUSY_MASK) ? -EBUSY : 0;
}

static int xiic_busy(struct xiic_i2c *i2c)
{
	int tries = 3;
	int err;

	if (i2c->tx_msg)
		return -EBUSY;

	/* for instance if previous transfer was terminated due to TX error
	 * it might be that the bus is on it's way to become available
	 * give it at most 3 ms to wake
	 */
	err = xiic_bus_busy(i2c);
	while (err && tries--) {
		msleep(1);
		err = xiic_bus_busy(i2c);
	}

	return err;
}

static void xiic_start_recv(struct xiic_i2c *i2c)
{
	u8 rx_watermark;
	struct i2c_msg *msg = i2c->rx_msg = i2c->tx_msg;

	/* Clear and enable Rx full interrupt. */
	xiic_irq_clr_en(i2c, XIIC_INTR_RX_FULL_MASK | XIIC_INTR_TX_ERROR_MASK);

	/* we want to get all but last byte, because the TX_ERROR IRQ is used
	 * to inidicate error ACK on the address, and negative ack on the last
	 * received byte, so to not mix them receive all but last.
	 * In the case where there is only one byte to receive
	 * we can check if ERROR and RX full is set at the same time
	 */
	rx_watermark = msg->len;
	if (rx_watermark > IIC_RX_FIFO_DEPTH)
		rx_watermark = IIC_RX_FIFO_DEPTH;
	xiic_setreg8(i2c, XIIC_RFD_REG_OFFSET, rx_watermark - 1);

	if (!(msg->flags & I2C_M_NOSTART))
		/* write the address */
		xiic_setreg16(i2c, XIIC_DTR_REG_OFFSET,
			(msg->addr << 1) | XIIC_READ_OPERATION |
			XIIC_TX_DYN_START_MASK);
	xiic_irq_clr_en(i2c, XIIC_INTR_BNB_MASK);

	xiic_setreg16(i2c, XIIC_DTR_REG_OFFSET,
		msg->len | ((i2c->nmsgs == 1) ? XIIC_TX_DYN_STOP_MASK : 0));
	if (i2c->nmsgs == 1)
		/* very last, enable bus not busy as well */
		xiic_irq_clr_en(i2c, XIIC_INTR_BNB_MASK);

	/* the message is tx:ed */
	i2c->tx_pos = msg->len;
}

static void xiic_start_send(struct xiic_i2c *i2c)
{
	struct i2c_msg *msg = i2c->tx_msg;

	xiic_irq_clr(i2c, XIIC_INTR_TX_ERROR_MASK);

	dev_dbg(i2c->adap.dev.parent, "%s entry, msg: %p, len: %d",
		__func__, msg, msg->len);
	dev_dbg(i2c->adap.dev.parent, "%s entry, ISR: 0x%x, CR: 0x%x\n",
		__func__, xiic_getreg32(i2c, XIIC_IISR_OFFSET),
		xiic_getreg8(i2c, XIIC_CR_REG_OFFSET));

	if (!(msg->flags & I2C_M_NOSTART)) {
		/* write the address */
		u16 data = ((msg->addr << 1) & 0xfe) | XIIC_WRITE_OPERATION |
			XIIC_TX_DYN_START_MASK;
		if ((i2c->nmsgs == 1) && msg->len == 0)
			/* no data and last message -> add STOP */
			data |= XIIC_TX_DYN_STOP_MASK;

		xiic_setreg16(i2c, XIIC_DTR_REG_OFFSET, data);
	}

	xiic_fill_tx_fifo(i2c);

	/* Clear any pending Tx empty, Tx Error and then enable them. */
	xiic_irq_clr_en(i2c, XIIC_INTR_TX_EMPTY_MASK | XIIC_INTR_TX_ERROR_MASK |
		XIIC_INTR_BNB_MASK);
}

static irqreturn_t xiic_isr(int irq, void *dev_id)
{
	struct xiic_i2c *i2c = dev_id;
	u32 pend, isr, ier;
	irqreturn_t ret = IRQ_NONE;
	/* Do not processes a devices interrupts if the device has no
 	 * interrupts pending
 	 */

	dev_dbg(i2c->adap.dev.parent, "%s entry\n", __func__);

	isr = xiic_getreg32(i2c, XIIC_IISR_OFFSET);
	ier = xiic_getreg32(i2c, XIIC_IIER_OFFSET);
	pend = isr & ier;
	if (pend || isr) {
		ret = IRQ_WAKE_THREAD;
	}

	return ret;
}

static void __xiic_start_xfer(struct xiic_i2c *i2c)
{
	int first = 1;
	int fifo_space = xiic_tx_fifo_space(i2c);
	dev_dbg(i2c->adap.dev.parent, "%s entry, msg: %p, fifos space: %d\n",
		__func__, i2c->tx_msg, fifo_space);

	if (!i2c->tx_msg)
		return;

	i2c->rx_pos = 0;
	i2c->tx_pos = 0;
	i2c->state = STATE_START;
	while ((fifo_space >= 2) && (first || (i2c->nmsgs > 1))) {
		if (!first) {
			i2c->nmsgs--;
			i2c->tx_msg++;
			i2c->tx_pos = 0;
		} else
			first = 0;

		if (i2c->tx_msg->flags & I2C_M_RD) {
			/* we dont date putting several reads in the FIFO */
			xiic_start_recv(i2c);
			return;
		} else {
			xiic_start_send(i2c);
			if (xiic_tx_space(i2c) != 0) {
				/* the message could not be completely sent */
				break;
			}
		}

		fifo_space = xiic_tx_fifo_space(i2c);
	}

	/* there are more messages or the current one could not be completely
 	 * put into the FIFO, also enable the half empty interrupt
 	 */
	if (i2c->nmsgs > 1 || xiic_tx_space(i2c))
		xiic_irq_clr_en(i2c, XIIC_INTR_TX_HALF_MASK);
}

static void xiic_start_xfer(struct xiic_i2c *i2c)
{
	mutex_lock(&i2c->lock);
	// xiic_reinit(i2c);
	__xiic_start_xfer(i2c);
	mutex_unlock(&i2c->lock);
}

static int xiic_xfer(struct i2c_adapter *adap, struct i2c_msg *msgs, int num)
{
	struct xiic_i2c *i2c = i2c_get_adapdata(adap);
	int err;
	int retry = 1000;
	irqreturn_t ret;

	dev_dbg(adap->dev.parent, "%s entry SR: 0x%x\n", __func__,
		xiic_getreg8(i2c, XIIC_SR_REG_OFFSET));

	err = xiic_busy(i2c);
	if (err) {
		err = -EBUSY;
		goto out;
	}

	i2c->tx_msg = msgs;
	i2c->nmsgs = num;

	xiic_setreg8(i2c, XIIC_CR_REG_OFFSET, 0);
	xiic_start_xfer(i2c);
	xiic_setreg8(i2c, XIIC_CR_REG_OFFSET, XIIC_CR_ENABLE_DEVICE_MASK);

	/* do polling instead of interrupt */
	do {
		msleep(1);
		retry--;
		ret =  xiic_isr(0, i2c);
		if (ret != IRQ_NONE) {
			(void) xiic_process(0, i2c);
		}
	} while (retry > 0 && i2c->state == STATE_START);

	if (i2c->state == STATE_DONE) {
		err = num;
	} else if (i2c->state == STATE_ERROR) {
		dev_dbg(adap->dev.parent, "xfer error");
		err = -EIO;
	} else {
		dev_dbg(adap->dev.parent, "xfer timeout");
		i2c->tx_msg = NULL;
		i2c->rx_msg = NULL;
		i2c->nmsgs = 0;
		err = -ETIMEDOUT;
		goto out;
	}

out:
	return err;
}

static u32 xiic_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

static int pca9548_select_chan(struct i2c_adapter *adap, u8 chan)
{
	struct i2c_msg msg;
	char buf[1];

	msg.addr = MGMT_XIIC_MUX_ADDR;
	msg.flags = 0;
	msg.len = 1;
	buf[0] = chan;
	msg.buf = buf;

	return xiic_xfer(adap, &msg, 1);
}

static u8 xiic_get_chan_id(u32 addr)
{
	int chan = 0;

	switch (addr) {
	case LM63_ADDR:
		chan = 1 << 3;
		break;
	default:
		break;
	}

	return chan;
}

static int xiic_mux_xfer(struct i2c_adapter *adap,
		struct i2c_msg *msgs, int num)
{
	struct xiic_i2c *i2c = i2c_get_adapdata(adap);
	u8 chan_id = 0;
	int ret;

	/* assume is there is only 1 level i2c-mux */
	if (num > 0) {
		chan_id = xiic_get_chan_id(msgs[0].addr);
		if (chan_id > 0 && i2c->last_chan != chan_id) {
			if (pca9548_select_chan(adap, chan_id) > 0) {
				i2c->last_chan = chan_id;
			}
		}
	}

	ret = xiic_xfer(adap, msgs, num);

	return ret;
}

static const struct i2c_algorithm xiic_algorithm = {
	.master_xfer = xiic_mux_xfer,
	.functionality = xiic_func,
};

static struct i2c_adapter xiic_adapter = {
	.owner = THIS_MODULE,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 8, 0)
	.class = I2C_CLASS_HWMON,
#else
	.class = I2C_CLASS_HWMON | I2C_CLASS_SPD,
#endif
	.algo = &xiic_algorithm,
};

static struct i2c_board_info lm96163_board_info = {
		I2C_BOARD_INFO("lm63", LM63_ADDR),
};

static int xiic_probe(struct platform_device *pdev)
{
	struct xiic_i2c *i2c;
	unsigned short thermal[] = {0x00, 0x32, 0x46, 0x50, 0x5a};
	unsigned short pwm[]    = {0x0E, 0x17, 0x1E, 0x23, 0x2E};
	uint16_t tach_value;
	uint8_t pwm_config_reg;
	union i2c_smbus_data data;
	struct resource *res;
	u32 sr;
	int i, ret;

	i2c = devm_kzalloc(&pdev->dev, sizeof(*i2c), GFP_KERNEL);
	if (!i2c) {
		return -ENOMEM;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		xocl_err(&pdev->dev, "resource is NULL");
		return -EINVAL;
	}
	platform_set_drvdata(pdev, i2c);
	xocl_info(&pdev->dev, "IO start: 0x%llx, end: 0x%llx",
		res->start, res->end);
	i2c->base = ioremap_nocache(res->start, res->end - res->start + 1);

	i2c->adap = xiic_adapter;
	snprintf(i2c->adap.name, sizeof(i2c->adap.name) - 1,
		"xclmgmt-i2c-%s", dev_name(&pdev->dev));
	i2c_set_adapdata(&i2c->adap, i2c);

	i2c->adap.dev.parent = &pdev->dev;


	mutex_init(&i2c->lock);
	init_waitqueue_head(&i2c->wait);

	xiic_reinit(i2c);

	/*
 	 * Detect endianness
 	 * Try to reset the TX FIFO. Then check the EMPTY flag. If it is not
 	 * set, assume that the endianness was wrong and swap.
 	 */
	i2c->endianness = LITTLE;
	xiic_setreg32(i2c, XIIC_CR_REG_OFFSET, XIIC_CR_TX_FIFO_RESET_MASK);
	/* Reset is cleared in xiic_reinit */
	sr = xiic_getreg32(i2c, XIIC_SR_REG_OFFSET);
	if (!(sr & XIIC_SR_TX_FIFO_EMPTY_MASK))
		i2c->endianness = BIG;


	/* add i2c adapter to i2c tree */
	ret = i2c_add_adapter(&i2c->adap);
	if (ret) {
		xocl_err(&pdev->dev, "add i2c adapter failed: %d\n", ret);
		goto failed;
	}

	ret = i2c_smbus_xfer(&i2c->adap, LM63_ADDR, 0,
		I2C_SMBUS_READ, LM63_REG_CONFIG_FAN,
		I2C_SMBUS_BYTE_DATA, &data);
	if (ret < 0) {
		xocl_warn(&pdev->dev, "read config fan failed");
		return 0;
	}
	pwm_config_reg = data.byte;

	xocl_info(&pdev->dev, "PWM_CONFIG_REG is :%x", pwm_config_reg);
	if (pwm_config_reg & LM63_REG_CONFIG_FAN_RD) {
		goto cont;
	}

	data.byte = LM63_REG_CONFIG_FAN_WR | LM63_REG_CONFIG_FAN_RD;
	i2c_smbus_xfer(&i2c->adap, LM63_ADDR, 0,
		I2C_SMBUS_WRITE, LM63_REG_CONFIG_FAN,
		I2C_SMBUS_BYTE_DATA, &data);

	data.byte = LM63_REG_PWM_VALUE_FULL;
	i2c_smbus_xfer(&i2c->adap, LM63_ADDR, 0,
		I2C_SMBUS_WRITE, LM63_REG_PWM_VALUE,
		I2C_SMBUS_BYTE_DATA, &data);

	for (i = 0; i < 5; i++) {
		data.byte = thermal[i];
		i2c_smbus_xfer(&i2c->adap, LM63_ADDR, 0,
			I2C_SMBUS_WRITE, LM63_REG_LUT_TEMP(i),
			I2C_SMBUS_BYTE_DATA, &data);

		data.byte = pwm[i];
		i2c_smbus_xfer(&i2c->adap, LM63_ADDR, 0,
			I2C_SMBUS_WRITE, LM63_REG_LUT_PWM(i),
			I2C_SMBUS_BYTE_DATA, &data);
	}

	/* check the fan's current tachometer value */
	data.byte = LM63_REG_CONFIG1_TACH;
	i2c_smbus_xfer(&i2c->adap, LM63_ADDR, 0,
		I2C_SMBUS_WRITE, LM63_REG_CONFIG1,
		I2C_SMBUS_BYTE_DATA, &data);

	/* read LSB first, to lock MSB */
	i2c_smbus_xfer(&i2c->adap, LM63_ADDR, 0,
		I2C_SMBUS_READ, LM63_REG_TACH_COUNT_LSB,
		I2C_SMBUS_BYTE_DATA, &data);
	tach_value = data.byte;

	/* read MSB */

	i2c_smbus_xfer(&i2c->adap, LM63_ADDR, 0,
		I2C_SMBUS_READ, LM63_REG_TACH_COUNT_MSB,
		I2C_SMBUS_BYTE_DATA, &data);
	tach_value |= ((uint16_t)data.byte) << 8;

	if(tach_value > MAX_TACH_THRESHOLD){
		xocl_info(&pdev->dev, "Detected fan's RPM is too slow %d",
			tach_value);
		data.byte = LM63_REG_CONFIG_FAN_DISABLE;
		i2c_smbus_xfer(&i2c->adap, LM63_ADDR, 0,
			I2C_SMBUS_WRITE, LM63_REG_CONFIG_FAN,
			I2C_SMBUS_BYTE_DATA, &data);
	} else {
		xocl_info(&pdev->dev, "Fan initialized, tach=0x%x",
			tach_value);
		data.byte = LM63_REG_CONFIG_FAN_RD;
		i2c_smbus_xfer(&i2c->adap, LM63_ADDR, 0,
			I2C_SMBUS_WRITE, LM63_REG_CONFIG_FAN,
			I2C_SMBUS_BYTE_DATA, &data);
	}

	/* set critical Temp */
	data.byte = LM63_REG_CONFIG1_TACH | LM63_REG_CONFIG1_TCRIT;
	i2c_smbus_xfer(&i2c->adap, LM63_ADDR, 0,
		I2C_SMBUS_WRITE, LM63_REG_CONFIG1,
		I2C_SMBUS_BYTE_DATA, &data);

	data.byte = 0x60;
	i2c_smbus_xfer(&i2c->adap, LM63_ADDR, 0,
		I2C_SMBUS_WRITE, LM63_REG_REMOTE_TCRIT,
		I2C_SMBUS_BYTE_DATA, &data);

cont:
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,8,0)
	i2c->lm63 = i2c_new_client_device(&i2c->adap, &lm96163_board_info);
#else
	i2c->lm63 = i2c_new_device(&i2c->adap, &lm96163_board_info);
#endif
	if (!i2c->lm63) {
		xocl_err(&pdev->dev, "add lm96163 failed \n");
	}

	return 0;

failed:
	xiic_deinit(i2c);
	devm_kfree(&pdev->dev, i2c);
	platform_set_drvdata(pdev, NULL);
	return ret;
}

static int __xiic_remove(struct platform_device *pdev)
{
	struct xiic_i2c *i2c;

	i2c = platform_get_drvdata(pdev);
	if (!i2c) {
		xocl_err(&pdev->dev, "driver data is NULL");
		return 0;
	}

	if (i2c->lm63) {
		i2c_unregister_device(i2c->lm63);
	}
	/* remove adapter & data */
	i2c_del_adapter(&i2c->adap);

	xiic_deinit(i2c);

	devm_kfree(&pdev->dev, i2c);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 11, 0)
static void xiic_remove(struct platform_device *pdev)
{
	__xiic_remove(pdev);
}
#else
#define xiic_remove __xiic_remove
#endif

struct platform_device_id xiic_id_table[] = {
	{ XOCL_DEVNAME(XOCL_XIIC), 0 },
	{ },
};

static struct platform_driver xiic_driver = {
	.probe		= xiic_probe,
	.remove		= xiic_remove,
	.driver		= {
		.name = XOCL_DEVNAME(XOCL_XIIC),
	},
	.id_table = xiic_id_table,
};

int __init xocl_init_xiic(void)
{
	return platform_driver_register(&xiic_driver);
}

void xocl_fini_xiic(void)
{
	platform_driver_unregister(&xiic_driver);
}
