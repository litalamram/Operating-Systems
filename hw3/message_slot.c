/*
 * message_slot.c
 *
 *  Created on: 27 Apr 2018
 *      Author: lital
 */

#undef __KERNEL__
#define __KERNEL__
#undef MODULE
#define MODULE

#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/string.h>

MODULE_LICENSE("GPL");

//Our custom definitions of IOCTL operations
#include "message_slot.h"

/**Represents a message slot*/
typedef struct msg_slot_t {
	char chanels[NUM_CHANELS][BUF_LEN];
	int lengths[NUM_CHANELS];
	int minor;
} MsgSlot;

/**Represents a node in linked list*/
typedef struct node_t {
	MsgSlot *data;
	struct node_t *next;
} Node;

/**Represents linked list*/
typedef struct list_t {
	Node *head;
} List;

//list of all the message slots were created
static List slotsLst;


//==================  LIST FUNCTIONS  ===========================

/**Creates a new message slot with the specified minor number
 * @return
 * on success, a new instance of MsgSlot,
 * on error, NULL*/
static MsgSlot* createMsgSlot (int minor){
	int i;
	MsgSlot *slot;
	slot = kmalloc(sizeof(MsgSlot), GFP_KERNEL);
	if (slot == NULL){
		return NULL;
	}
	for (i=0; i<NUM_CHANELS; i++){
		slot->lengths[i] = -1;
	}
	slot->minor = minor;
	return slot;
}

/**Creates a new instance of node
 * and sets its data to a new instance of a MsgSlot with the specified minor
 * @return
 * on success, a new instance of Node,
 * on error, NULL*/
static Node* createNode (int minor){
	Node *n;
	MsgSlot *slot;
	n = kmalloc(sizeof(Node), GFP_KERNEL);
	if (n == NULL){
		return NULL;
	}
	slot = createMsgSlot(minor);
	n->data = slot;
	n->next = NULL;
	return n;
}

/**Adds the specified node to the specified linked list*/
static void addNodeToLst (List *lst, Node *n){
	if (lst->head == NULL){
		lst->head = n;
		return;
	}
	n->next = lst->head;
	lst->head = n;
}

/**Returns a MsgSlot with the specified minor in the list,
 * or NULL if one does not exist*/
static MsgSlot* getSlotByMinor (List lst, int minor){
	Node *curr;
	for (curr = lst.head; curr != NULL; curr = curr->next){
		if (curr->data->minor == minor){
			return curr->data;
		}
	}
	return NULL;
}

/**Frees all resources associeted with the list recoursively*/
static void releaseLstRec(Node *n){
	if (n != NULL){
		releaseLstRec(n->next);
		kfree(n->data);
		kfree(n);
	}
}

/**Frees all the resources associated with the list*/
static void releaseLst (List lst){
	releaseLstRec(lst.head);
}

//================== DEVICE FUNCTIONS ===========================

/**Creates a new message slot with the specified minor if one doesn't exist,
 * and adds it to the slotLst
 * @return
 * on success, SUCCESS
 * on error, -1 and errno is set to ENOMEM*/
static int device_open( struct inode* inode, struct file*  file ) {
	int minor = iminor(inode);
	Node *n;
	if (NULL == getSlotByMinor(slotsLst, minor)){ //a MsgSlot with specified minor doesn't exist
		n = createNode(minor);
		if (n == NULL){ //error creating the node
			return -ENOMEM;
		}
		addNodeToLst(&slotsLst, n);
	}
	file->private_data = (void*)-1; //init the channel to -1
	return SUCCESS;
}

//---------------------------------------------------------------
static int device_release( struct inode* inode, struct file*  file) {
	return SUCCESS;
}

//---------------------------------------------------------------
/**Returns the last message written on the chanel
 * @return
 * if no chanel has been set, returns -1 andd errno is set to EINVAL
 * If no message exists on the channel, returns -1 and sets errno to EWOULDBLOCK
 * If the provided user space buffer is too small to hold the message, returns -1 and sets errno to ENOSPC
 * if an error occured while reading the message, returns -1 and sets errno to EINVAL
 * If the provided user buffer is NULL, returns -1 and sets errno to EINVAL
 * otherwise, returns the number of bytes were read*/
static ssize_t device_read(struct file* file, char __user* buffer, size_t length, loff_t*  offset )
{
	int chanel, minor ,message_length, i;
	MsgSlot *slot;
	chanel = (int) file->private_data;

	if (chanel == -1) { //chanel hasn't been set
		return -EINVAL;
	}
	minor = iminor(file_inode(file));
	slot = getSlotByMinor(slotsLst, minor);
	message_length = slot->lengths[chanel];

	if (message_length == -1){ //no messsage exist on the chanel
		return -EWOULDBLOCK;
	}

	if (length < message_length){ //user buffer is too small
		return -ENOSPC;
	}

	if (buffer == NULL){ //provided user buffer is NULL
		return -EINVAL;
	}

	for (i=0; i<message_length; i++){ //read the message
		if (put_user(slot->chanels[chanel][i], &buffer[i]) != 0){
			return -EINVAL;
		}
	}
	return message_length;
}

//---------------------------------------------------------------
/**Writes a message to the channel.
 * @return
 * If no channel has been set, returns -1 and sets errno to EINVAL
 * If the length of the message is more than BUF_LEN bytes, returns -1 and sets errno to EINVAL
 * If the provided user buffer is NULL, returns -1 and sets errno to EINVAL
 * if an error occured while writing the message, returns -1 and sets errno to EINVAL
 * otherwise, returns the number of bytes were written*/
static ssize_t device_write(struct file* file, const char __user* buffer, size_t length, loff_t* offset) {
	int chanel, minor, i;
	MsgSlot *slot;
	chanel = (int) file->private_data;

	if (chanel == -1){ //chanel hasn't been set
		return -EINVAL;
	}

	minor = iminor(file_inode(file));
	slot = getSlotByMinor(slotsLst, minor);

	if (length > BUF_LEN){ //message too big
		return -EINVAL;
	}

	if (buffer == NULL){ //provided user buffer is NULL
		return -EINVAL;
	}

	//write the message
	slot->lengths[chanel] = 0;
	for (i=0; i<length; i++){
		if (get_user(slot->chanels[chanel][i], &buffer[i]) != 0){
			slot->lengths[chanel] = -1;
			return -EINVAL;
		}
		slot->lengths[chanel]++;
	}

	return length;
}

//----------------------------------------------------------------
/**Sets the file descriptor's channel id
 * @return
 * If the passed command is not MSG_SLOT_CHANNEL, returns -1 and sets errno to EINVAL
 * If the passed channel ID in not of [0,1,2,3], returns -1 and sets errno to EINVAL
 * Otherwise, SUCESS*/
static long device_ioctl( struct file* file, unsigned int ioctl_command_id, unsigned long  ioctl_param ) {
	if( MSG_SLOT_CHANEL != ioctl_command_id ){
		return -EINVAL;
	}
	if (ioctl_param < 0 || ioctl_param > 3){
		return -EINVAL;
	}

	file->private_data = (void*)ioctl_param;

	return SUCCESS;
}

//==================== DEVICE SETUP =============================

// Holds the functions to be called
// when a process does something to the device
struct file_operations Fops =
{
		.read           = device_read,
		.write          = device_write,
		.open           = device_open,
		.unlocked_ioctl = device_ioctl,
		.release        = device_release,
};

//---------------------------------------------------------------

/** Initialize the module - Register the character device*/
static int __init simple_init(void)
{
	int rc = -1;

	// Register driver capabilities. Obtain major num
	rc = register_chrdev( MAJOR_NUM, DEVICE_RANGE_NAME, &Fops);

	if( rc < 0 ) { //an error registering the character device
		printk( KERN_ALERT "%s registraion failed for  %d\n", DEVICE_RANGE_NAME, MAJOR_NUM );
		return rc;
	}

	printk(KERN_INFO "message_slot: registered major number %d\n", MAJOR_NUM);

	return 0;
}

//---------------------------------------------------------------
/**Unregisters the character device and frees all memory has been allocated*/
static void __exit simple_cleanup(void) {
	// Unregister the device
	// Should always succeed
	unregister_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME);
	//free memory
	releaseLst(slotsLst);
}

//---------------------------------------------------------------
module_init(simple_init);
module_exit(simple_cleanup);

//========================= END OF FILE =========================

