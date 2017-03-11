#include <minix/drivers.h>
#include <minix/chardriver.h>
#include <sys/ioc_homework.h>
#include <stdio.h>
#include <stdlib.h>
#include <minix/ds.h>
#include "homework.h"

/*
 * Function prototypes for the homework driver.
 */
static int homework_open(devminor_t minor, int access, endpoint_t user_endpt);
static int homework_close(devminor_t minor);
static ssize_t homework_read(devminor_t minor, u64_t position, endpoint_t endpt,
    cp_grant_id_t grant, size_t size, int flags, cdev_id_t id);
static ssize_t homework_write(devminor_t minor, u64_t position, endpoint_t endpt,
	cp_grant_id_t grant, size_t size, int UNUSED(flags),
	cdev_id_t UNUSED(id));
static int homework_ioctl(devminor_t minor, unsigned long request, endpoint_t endpt,
	cp_grant_id_t grant, int flags, endpoint_t user_endpt, cdev_id_t id);

/* SEF functions and variables. */
static void sef_local_startup(void);
static int sef_cb_init(int type, sef_init_info_t *info);
static int sef_cb_lu_state_save(int);
static int lu_state_restore(void);

/* Entry points to the homework driver. */
static struct chardriver homework_tab =
{
    .cdr_open	= homework_open,
    .cdr_close	= homework_close,
    .cdr_read	= homework_read,
    .cdr_write	= homework_write,
    .cdr_ioctl  = homework_ioctl
};

/** State variable to count the number of times the device has been opened.
 * Note that this is not the regular type of open counter: it never decreases.
 */
static int open_counter;
/** State variable to store an integer written to this pseudo-driver
 * Note that this is an array of 4 integers and is initialized to 0
 */
static int slots[4] = { 0 };
/** State variable to store which integer slot is currently available for reading
 * Note that this integer is initialized to 0
 */
static int slot_in_use = 0;

/*===========================================================================*
 *    homework_open                                                     *
 *    -Print opening, do no setup
 *===========================================================================*/
static int homework_open(devminor_t UNUSED(minor), int UNUSED(access),
    endpoint_t UNUSED(user_endpt))
{
    printf("homework_open(). Called %d time(s).\n", ++open_counter);
    return OK;
}

/*===========================================================================*
 *    homework_close                                                     *
 *    -Print closing, do no cleanup
 *===========================================================================*/
static int homework_close(devminor_t UNUSED(minor))
{
    printf("homework_close()\n");
    return OK;
}

/*===========================================================================*
 *    homework_read                                                     *
 *    -Print reading, return integer in slot available
 *===========================================================================*/
static ssize_t homework_read(devminor_t UNUSED(minor), u64_t UNUSED(position),
    endpoint_t endpt, cp_grant_id_t grant, size_t size, int UNUSED(flags),
    cdev_id_t UNUSED(id))
{
    printf("homework_read()\n");

    int *ptr = slots + slot_in_use;
    int ret;
    const int integer_size = 4;
    //char *buf = HOMEWORK_MESSAGE;
    if (size < 4)
    { 
            printf("homework_read(): Read MUST be 4 bytes\n");
            return EINVAL;
    }

    /* Copy the requested part to the caller. */
    ret = sys_safecopyto(endpt, grant, 0, (vir_bytes) ptr, integer_size);
    return (ret != OK) ? ret : integer_size;
}

/*===========================================================================*
 *    homework_write                                                     *
 *    -Print writing, write integer to slot available
 *===========================================================================*/
static ssize_t homework_write(devminor_t minor, u64_t position, endpoint_t endpt,
	cp_grant_id_t grant, size_t size, int UNUSED(flags),
	cdev_id_t UNUSED(id))
	{
    printf("homework_write()\n");

    int ret;
    int *ptr = slots + slot_in_use;
    const int integer_size = 4;

    //char *buf = HOMEWORK_MESSAGE;
    if (size < 4)
    { 
            printf("homework_write(): Write MUST be 4 bytes\n");
            return EINVAL;
    }

    /* Copy the requested part from the caller to the device */
    ret = sys_safecopyfrom(endpt, grant, 0, (vir_bytes) ptr, size);
    return (ret != OK) ? ret : integer_size;
}

/*===========================================================================*
 *    homework_ioctl                                                     *
 *    -Print ioctl_fcn called (HIOCSLOT, HIOCCSLOT, HIOCGETSLOT), change, clear, or get available slot
 *===========================================================================*/
static int homework_ioctl(devminor_t minor, unsigned long request, endpoint_t endpt,
	cp_grant_id_t grant, int flags, endpoint_t user_endpt, cdev_id_t id)
{
    printf("flags = %d", flags);
    printf("flags = %d", flags);

	switch (request) {
		case HIOCSLOT:
                printf("homework_ioctl() HIOCSLOT\n");
                printf("Setting slot to %d", k
                return EXIT_SUCCESS;
		case HIOCCLEARSLOT:
                printf("homework_ioctl() HIOCCLEARSLOT\n");
                return EXIT_SUCCESS;
		case HIOCGETSLOT:
                printf("homework_ioctl() HIOCGETSLOT\n");
                return EXIT_SUCCESS;
		default:
			break;
	}

	return ENOTTY;
}

static int sef_cb_lu_state_save(int UNUSED(state)) {
/* Save the state. */
    ds_publish_u32("open_counter", open_counter, DSF_OVERWRITE);

    return OK;
}

static int lu_state_restore() {
/* Restore the state. */
    u32_t value;

    ds_retrieve_u32("open_counter", &value);
    ds_delete_u32("open_counter");
    open_counter = (int) value;

    return OK;
}

static void sef_local_startup()
{
    /*
     * Register init callbacks. Use the same function for all event types
     */
    sef_setcb_init_fresh(sef_cb_init);
    sef_setcb_init_lu(sef_cb_init);
    sef_setcb_init_restart(sef_cb_init);

    /*
     * Register live update callbacks.
     */
    /* - Agree to update immediately when LU is requested in a valid state. */
    sef_setcb_lu_prepare(sef_cb_lu_prepare_always_ready);
    /* - Support live update starting from any standard state. */
    sef_setcb_lu_state_isvalid(sef_cb_lu_state_isvalid_standard);
    /* - Register a custom routine to save the state. */
    sef_setcb_lu_state_save(sef_cb_lu_state_save);

    /* Let SEF perform startup. */
    sef_startup();
}

static int sef_cb_init(int type, sef_init_info_t *UNUSED(info))
{
/* Initialize the homework driver. */
    int do_announce_driver = TRUE;

    open_counter = 0;
    switch(type) {
        case SEF_INIT_FRESH:
            printf("%s", HOMEWORK_MESSAGE);
        break;

        case SEF_INIT_LU:
            /* Restore the state. */
            lu_state_restore();
            do_announce_driver = FALSE;

            printf("%sHey, I'm a new version!\n", HOMEWORK_MESSAGE);
        break;

        case SEF_INIT_RESTART:
            printf("%sHey, I've just been restarted!\n", HOMEWORK_MESSAGE);
        break;
    }

    /* Announce we are up when necessary. */
    if (do_announce_driver) {
        chardriver_announce();
    }

    /* Initialization completed successfully. */
    return OK;
}

int main(void)
{
    /*
     * Perform initialization.
     */
    sef_local_startup();

    /*
     * Run the main loop.
     */
    chardriver_task(&homework_tab);
    return OK;
}

