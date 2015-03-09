/**
 * @file mac_disassociate.c
 *
 * @brief Implements the MLME-DISASSOCIATION functionality
 *
 * Copyright (c) 2013-2015 Atmel Corporation. All rights reserved.
 *
 * \asf_license_start
 *
 * \page License
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. The name of Atmel may not be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * 4. This software may only be redistributed and used in connection with an
 *    Atmel microcontroller product.
 *
 * THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * EXPRESSLY AND SPECIFICALLY DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * \asf_license_stop
 *
 */

/*
 * Copyright (c) 2014-2015 Atmel Corporation. All rights reserved.
 *
 * Licensed under Atmel's Limited License Agreement --> EULA.txt
 */

/* === Includes ============================================================ */
#include <compiler.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "return_val.h"
#include "pal.h"
#include "bmm.h"
#include "qmm.h"
#include "tal.h"
#include "ieee_const.h"
#include "mac_msg_const.h"
#include "mac_api.h"
#include "mac_msg_types.h"
#include "mac_data_structures.h"
#include "stack_config.h"
#include "mac_internal.h"
#include "mac.h"
#include "mac_build_config.h"

#if (MAC_DISASSOCIATION_BASIC_SUPPORT == 1)

/* === Macros =============================================================== */

/*
 * Disassociation payload length
 */
#define DISASSOC_PAYLOAD_LEN             (2)

/* === Globals ============================================================= */

/* === Prototypes ========================================================== */

static bool mac_awake_disassociate(buffer_t *buf_ptr);

static void mac_gen_mlme_disassociate_conf(buffer_t *buf,
		uint8_t status,
		uint8_t dev_addr_mode,
		uint16_t dev_panid,
		address_field_t *dev_addr);

/* === Implementation ====================================================== */

/**
 * @brief Initiates MLME disassociate confirm message
 *
 * This function creates and appends a MLME disassociate confirm message
 * into the  internal event queue.
 *
 * @param buf Buffer for sending MLME disassociate confirm message to NHLE
 * @param status Status of disassociation
 * @param dev_addr_mode Addressing mode of the device that has either requested
 *                      disassociation or been instructed to disassociate by
 *                      its coordinator.
 * @param dev_panid PAN identifier of the device that has either requested
 *                  disassociation or been instructed to disassociate by its
 *                  coordinator.
 * @param dev_addr Address of the device that has either requested
 *                 disassociation or been instructed to disassociate by its
 *                 coordinator.
 */
static void mac_gen_mlme_disassociate_conf(buffer_t *buf,
		uint8_t status,
		uint8_t dev_addr_mode,
		uint16_t dev_panid,
		address_field_t *dev_addr)
{
	mlme_disassociate_conf_t *mdc;

	mdc = (mlme_disassociate_conf_t *)BMM_BUFFER_POINTER(buf);

	mdc->cmdcode = MLME_DISASSOCIATE_CONFIRM;
	mdc->status = status;
	mdc->DeviceAddrMode = dev_addr_mode;
	mdc->DevicePANId = dev_panid;

	if (FCF_SHORT_ADDR == dev_addr_mode) {
		/* Clean-up of 64 bit address before writing 16 bit value. */
		mdc->DeviceAddress = 0;
		/* A short device address is requested. */
		ADDR_COPY_DST_SRC_16(mdc->DeviceAddress,
				dev_addr->short_address);
	} else {
		/* A long device address is requested. */
		ADDR_COPY_DST_SRC_64(mdc->DeviceAddress,
				dev_addr->long_address);
	}

	qmm_queue_append(&mac_nhle_q, (buffer_t *)buf);
}

/**
 * @brief Handles the MLME disassociate request command from the NWK layer
 *
 * The MLME-DISASSOCIATE.request primitive is generated by the next
 * higher layer of an associated device and issued to its MLME to
 * request disassociation from the PAN. It is also generated by the
 * next higher layer of the coordinator and issued to its MLME to
 * instruct an associated device to leave the PAN.
 *
 * @param m Pointer to the MLME-DISASSOCIATION.Request message passed by NHLE
 */
void mlme_disassociate_request(uint8_t *m)
{
	mlme_disassociate_req_t disassoc_req;

	frame_info_t *transmit_frame
		= (frame_info_t *)BMM_BUFFER_POINTER((buffer_t *)m);

	/* Store the disassociation request received from NHLE. */
	memcpy((uint8_t *)&disassoc_req, (uint8_t *)transmit_frame,
			sizeof(mlme_disassociate_req_t));

#ifndef REDUCED_PARAM_CHECK
	if (disassoc_req.DevicePANId != tal_pib.PANId) {
		mac_gen_mlme_disassociate_conf((buffer_t *)m,
				MAC_INVALID_PARAMETER,
				disassoc_req.DeviceAddrMode,
				disassoc_req.DevicePANId,
				(address_field_t *)&disassoc_req.DeviceAddress);
		return;
	}
#endif  /* REDUCED_PARAM_CHECK */

	/* Now build the actual disassociation command frame. */
	{
		uint8_t frame_len;
		uint8_t *frame_ptr;
		uint8_t *temp_frame_ptr;
		uint16_t fcf;

		transmit_frame->buffer_header = (buffer_t *)m;
		transmit_frame->msg_type = DISASSOCIATIONNOTIFICATION;

		/* Get the payload pointer. */
		frame_ptr = temp_frame_ptr
					= (uint8_t *)transmit_frame +
						LARGE_BUFFER_SIZE -
						DISASSOC_PAYLOAD_LEN - 2; /* Add
		                                                           * 2
		                                                           *
		                                                           *octets
		                                                           * for
		                                                           *
		                                                           *FCS.
		                                                           **/

		/* Update the payload field. */
		*frame_ptr++ = DISASSOCIATIONNOTIFICATION;

		/* Set up the disassociation reason code. */
		*frame_ptr = disassoc_req.DisassociateReason;

		/* Get the payload pointer again to add the MHR. */
		frame_ptr = temp_frame_ptr;

		/* Update the length. */
		frame_len = DISASSOC_PAYLOAD_LEN +
				2 + /* Add 2 octets for FCS */
				2 + /* 2 octets for Destination PAN-Id */
				2 + /* 2 octets for short Destination Address */
				8 + /* 8 octets for long Source Address */
				3; /* 3 octets DSN and FCF */

		/* Source address */
		frame_ptr -= 8;
		convert_64_bit_to_byte_array(tal_pib.IeeeAddress, frame_ptr);

		/* Destination address */
		if (FCF_SHORT_ADDR == disassoc_req.DeviceAddrMode) {
			frame_ptr -= 2;
			convert_16_bit_to_byte_address(
					disassoc_req.DeviceAddress, frame_ptr);
		} else {
			frame_ptr -= 8;
			frame_len += 6;
			convert_64_bit_to_byte_array(disassoc_req.DeviceAddress,
					frame_ptr);
		}

		/* Destination PAN-Id */
		frame_ptr -= 2;
		convert_16_bit_to_byte_array(tal_pib.PANId, frame_ptr);

		/* Set DSN. */
		frame_ptr--;
		*frame_ptr = mac_pib.mac_DSN++;

		/* Construct FCF. */
		/* 802.15.4-2006 sets the PAN-Id compression) bit. */
		fcf = FCF_SET_FRAMETYPE(FCF_FRAMETYPE_MAC_CMD) |
				FCF_SET_DEST_ADDR_MODE(
				disassoc_req.DeviceAddrMode) |
				FCF_SET_SOURCE_ADDR_MODE(FCF_LONG_ADDR) |
				FCF_PAN_ID_COMPRESSION |
				FCF_ACK_REQUEST;

		/* Set the FCF. */
		frame_ptr -= 2;
		convert_spec_16_bit_to_byte_array(fcf, frame_ptr);

		/* First element shall be length of PHY frame. */
		frame_ptr--;
		*frame_ptr = frame_len;

		/* Finished building of frame. */
		transmit_frame->mpdu = frame_ptr;
	}

#if (MAC_DISASSOCIATION_FFD_SUPPORT == 1)
	/* Indirect transmission not ongoing yet. */
	transmit_frame->indirect_in_transit = false;

	if (
		((MAC_PAN_COORD_STARTED == mac_state) ||
		(MAC_COORDINATOR == mac_state)) &&
		(disassoc_req.TxIndirect)
		) {
		/*
		 * The current device is a coordinator, and if the DeviceAddress
		 * is not ours, then send via indirect transmission.
		 */
		if (
			((disassoc_req.DeviceAddrMode == WPAN_ADDRMODE_SHORT) &&
			(disassoc_req.DeviceAddress != macCoordShortAddress)) ||
			((disassoc_req.DeviceAddrMode == WPAN_ADDRMODE_LONG) &&
			(disassoc_req.DeviceAddress !=
			mac_pib.mac_CoordExtendedAddress))
			) {
			/* Append the data into indirect queue. */
#ifdef ENABLE_QUEUE_CAPACITY
			if (QUEUE_FULL ==
					qmm_queue_append(&indirect_data_q,
					(buffer_t *)m)) {
				/*
				 * If there is no capacity to store the
				 * transaction, the MLME
				 * will discard the MSDU and issue the
				 * MLME-DISASSOCIATE.
				 * confirm primitive with a status of
				 * TRANSACTION_OVERFLOW.
				 */
				mac_gen_mlme_disassociate_conf((buffer_t *)m,
						MAC_TRANSACTION_OVERFLOW,
						disassoc_req.DeviceAddrMode,
						disassoc_req.DevicePANId,
						(address_field_t *)&disassoc_req.DeviceAddress);
				return;
			}

#else
			qmm_queue_append(&indirect_data_q, (buffer_t *)m);
#endif  /* ENABLE_QUEUE_CAPACITY */

			/*
			 * If an FFD does have pending data,
			 * the MAC persistence timer needs to be started.
			 */
			transmit_frame->persistence_time
				= mac_pib.mac_TransactionPersistenceTime;
			mac_check_persistence_timer();
		}

#ifndef REDUCED_PARAM_CHECK
		else {
			mac_gen_mlme_disassociate_conf((buffer_t *)m,
					MAC_INVALID_PARAMETER,
					disassoc_req.DeviceAddrMode,
					disassoc_req.DevicePANId,
					(address_field_t *)&disassoc_req.DeviceAddress);
			return;
		}
#endif  /* REDUCED_PARAM_CHECK */
	} else /* Device */
#endif /* (MAC_DISASSOCIATION_FFD_SUPPORT == 1) */
	{
		bool transmission_status;

		/* Wake up the radio first */
		mac_trx_wakeup();

		transmission_status = mac_awake_disassociate((buffer_t *)m);

		if (!transmission_status) {
			/* Create the MLME DISASSOCIATION confirmation message
			**/
			mac_gen_mlme_disassociate_conf((buffer_t *)m,
					MAC_CHANNEL_ACCESS_FAILURE,
					disassoc_req.DeviceAddrMode,
					disassoc_req.DevicePANId,
					(address_field_t *)&disassoc_req.DeviceAddress);

			/* Set radio to sleep if allowed */
			mac_sleep_trans();
		}
	}
} /* mlme_disassociate_request() */

/*
 * @brief Continues handling MLME-DISASSOCIATE.request once the radio is awake
 *
 * This function sends the disassociation request information to TAL. On
 * sending the disassociation request all information about its parent device
 * is cleared.
 *
 * @param m Pointer to the buffer to be sent to TAL.
 *
 * @return True if the frame is transmitted successfully, false otherwise.
 */
static bool mac_awake_disassociate(buffer_t *buf_ptr)
{
	frame_info_t *transmit_frame;
	transmit_frame = (frame_info_t *)BMM_BUFFER_POINTER(buf_ptr);

	/*
	 * Send the disassociation information to the TAL.
	 * Start CSMA-CA using backoff and retry (direct transmission).
	 */
	retval_t tal_tx_status;

#ifdef BEACON_SUPPORT
	csma_mode_t cur_csma_mode;

	if (NON_BEACON_NWK == tal_pib.BeaconOrder) {
		/* In Nonbeacon network the frame is sent with unslotted
		 * CSMA-CA. */
		cur_csma_mode = CSMA_UNSLOTTED;
	} else {
		/* In Beacon network the frame is sent with slotted CSMA-CA. */
		cur_csma_mode = CSMA_SLOTTED;
	}

	tal_tx_status = tal_tx_frame(transmit_frame, cur_csma_mode, true);
#else   /* No BEACON_SUPPORT */
	/* In Nonbeacon build the frame is sent with unslotted CSMA-CA. */
	tal_tx_status = tal_tx_frame(transmit_frame, CSMA_UNSLOTTED, true);
#endif  /* BEACON_SUPPORT / No BEACON_SUPPORT */

	if (MAC_SUCCESS == tal_tx_status) {
		MAKE_MAC_BUSY();
	} else {
		return false;
	}

	return true;
}

/**
 * @brief Process a disassociation notification command
 *
 * This functions processes a received disassociation notification
 * command frame.
 * Actual data are taken from the incoming frame in mac_parse_buffer.
 *
 * @param msg Frame buffer to be filled in
 */
void mac_process_disassociate_notification(buffer_t *msg)
{
	mlme_disassociate_ind_t *dai
		= (mlme_disassociate_ind_t *)BMM_BUFFER_POINTER(
			((buffer_t *)msg));

	/* Set up the header portion of the mlme_disassociate_ind_t. */
	dai->cmdcode = MLME_DISASSOCIATE_INDICATION;

	/* Build the indication parameters. */

	/*
	 * Set the DeviceAddress first. The device address is the address
	 * of the device requesting the disassociaton which is always
	 * contained in the source address.
	 */
	ADDR_COPY_DST_SRC_64(dai->DeviceAddress,
			mac_parse_data.src_addr.long_address);

	dai->DisassociateReason
		= mac_parse_data.mac_payload_data.disassoc_req_data.
			disassoc_reason;

	qmm_queue_append(&mac_nhle_q, (buffer_t *)msg);

	/*
	 * Once a device is disassociated from a coordinator, the coordinator's
	 * address info should be cleared.
	 */
	memset((uint8_t *)&mac_pib.mac_CoordExtendedAddress, 0,
			sizeof(mac_pib.mac_CoordExtendedAddress));
	/* mac_pib.mac_CoordExtendedAddress = (uint64_t)CLEAR_ADDR_64; */

	/* The default short address is 0xFFFF. */
	mac_pib.mac_CoordShortAddress = INVALID_SHORT_ADDRESS;
}

/**
 * @brief Prepares a disassociation confirm message with device address
 * information
 *
 * This functions prepares a disassociation confirm message in case the device
 * address information needs to be extracted.
 *
 * @param buf Buffer for sending MLME disassociate confirm message to NHLE
 * @param status Status of disassociation
 */
void mac_prep_disassoc_conf(buffer_t *buf,
		uint8_t status)
{
	frame_info_t *frame_ptr = (frame_info_t *)BMM_BUFFER_POINTER(buf);

#if (MAC_DISASSOCIATION_FFD_SUPPORT == 1)
	uint8_t dis_dest_addr_mode;
	uint64_t temp_dev_addr = 0;

	dis_dest_addr_mode
		= (((frame_ptr->mpdu[PL_POS_FCF_2]) >>
			FCF_2_DEST_ADDR_OFFSET) & 3);

	if ((FCF_SHORT_ADDR == dis_dest_addr_mode) ||
			(FCF_LONG_ADDR  == dis_dest_addr_mode)) {
		temp_dev_addr
			= convert_byte_array_to_64_bit(&(frame_ptr->mpdu[
					PL_POS_DST_ADDR_START
				]));
	}

	if (MAC_PAN_COORD_STARTED == mac_state) {
		/*
		 * For PAN coordinator fill parameters of device that
		 * we requested to disassociate into the disassociation confirm
		 * message.
		 * Since we have transmitted the disassociation notification
		 * frame
		 * ourvelves, the destination address information is to be used
		 * here.
		 */
		if (FCF_SHORT_ADDR == dis_dest_addr_mode) {
			mac_gen_mlme_disassociate_conf((buffer_t *)buf,
					status,
					dis_dest_addr_mode,
					tal_pib.PANId,
					(address_field_t *)&temp_dev_addr);
		} else {
			mac_gen_mlme_disassociate_conf((buffer_t *)buf,
					status,
					dis_dest_addr_mode,
					tal_pib.PANId,
					(address_field_t *)&temp_dev_addr);
		}
	} else if (MAC_COORDINATOR == mac_state) {
		/* We are a coordinator. */

		/*
		 * There are 2 potential choices for a coordinator to get here:
		 * 1) We have requested ourselves to disassociate with our own
		 * parent.
		 * 2) We have requested one of our children (other coordinators
		 * or
		 *    end devices) to leave the network.
		 */

		/*
		 * For case 1) check whether the destination address of the
		 * disassociation notification frame is matching the address
		 * of our parent.
		 */
		if (

		        /*
		         * We had requested to disassociate from our parent
		         * using the
		         * short address of the parent.
		         */
			(
				(FCF_SHORT_ADDR == dis_dest_addr_mode) &&
				(convert_byte_array_to_16_bit(&frame_ptr->mpdu[
					PL_POS_DST_ADDR_START]) ==
				mac_pib.mac_CoordShortAddress)
			) ||

		        /*
		         * We had requested to disassociate from our parent
		         * using the
		         * extended address of the parent.
		         */
			(
				(FCF_LONG_ADDR == dis_dest_addr_mode) &&
				convert_byte_array_to_64_bit(&frame_ptr->mpdu[
					PL_POS_DST_ADDR_START]) ==
				mac_pib.mac_CoordExtendedAddress
			)
			) {
			/*
			 * We are acting as a child here, so we need to fill in
			 * our
			 * own device parameter into the disassociation confirm
			 * message.
			 */
			if ((BROADCAST == tal_pib.ShortAddress) ||
					(CCPU_ENDIAN_TO_LE16(
					MAC_NO_SHORT_ADDR_VALUE) ==
					tal_pib.ShortAddress)
					) {
				/* We have no valid short address. */
				mac_gen_mlme_disassociate_conf((buffer_t *)buf,
						status,
						FCF_LONG_ADDR,
						tal_pib.PANId,
						(address_field_t *)&tal_pib.IeeeAddress);
			} else {
				/* We have a valid short address. */
				mac_gen_mlme_disassociate_conf((buffer_t *)buf,
						status,
						FCF_SHORT_ADDR,
						tal_pib.PANId,
						(address_field_t *)&tal_pib.ShortAddress);
			}
		} else {
			/*
			 * We are acting as a parent here and have requested one
			 * of our
			 * children to leave the network.
			 * For coordinators that are disassociating their
			 * children.
			 * fill parameters of device/child that we requested to
			 * disassociate into the disassociation confirm message.
			 * Since we have transmitted the disassociation
			 * notification frame
			 * ourvelves, the destination address information is to
			 * be used here.
			 */
			mac_gen_mlme_disassociate_conf((buffer_t *)buf,
					status,
					dis_dest_addr_mode,
					tal_pib.PANId,
					(address_field_t *)&temp_dev_addr);
		}
	} else
#endif /* (MAC_DISASSOCIATION_FFD_SUPPORT == 1) */
	{
		/*
		 * We are an end device, so we need to fill in our own device
		 * parameter into the disassociation confirm message.
		 */
		if ((BROADCAST == tal_pib.ShortAddress) ||
				(CCPU_ENDIAN_TO_LE16(MAC_NO_SHORT_ADDR_VALUE) ==
				tal_pib.ShortAddress)
				) {
			/* We have no valid short address. */
			mac_gen_mlme_disassociate_conf((buffer_t *)buf,
					status,
					FCF_LONG_ADDR,
					tal_pib.PANId,
					(address_field_t *)&tal_pib.IeeeAddress);
		} else {
			/* We have a valid short address. */
			mac_gen_mlme_disassociate_conf((buffer_t *)buf,
					status,
					FCF_SHORT_ADDR,
					tal_pib.PANId,
					(address_field_t *)&tal_pib.ShortAddress);
		}
	}

	frame_ptr = frame_ptr; /* Keep compiler happy. */
}

#endif  /* (MAC_DISASSOCIATION_BASIC_SUPPORT == 1) */

/* EOF */
