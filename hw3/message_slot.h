/*
 * message_slot.h
 *
 *  Created on: 27 Apr 2018
 *      Author: lital
 */

#ifndef MESSAGE_SLOT_H_
#define MESSAGE_SLOT_H_


#include <linux/ioctl.h>

// The major device number.
#define MAJOR_NUM 244

// Set the file descriptor's channel id
#define MSG_SLOT_CHANEL _IOW(MAJOR_NUM, 0, unsigned int)

#define DEVICE_RANGE_NAME "message_slot_dev"
#define BUF_LEN 128
#define NUM_CHANELS 4
#define SUCCESS 0



#endif /* MESSAGE_SLOT_H_ */
