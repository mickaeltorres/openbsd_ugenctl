#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <err.h>
#include <dev/usb/usb.h>

struct usb_change_list {
	u_short vid[USB_DEVICE_UGEN_FORCE_MAX];
	u_short pid[USB_DEVICE_UGEN_FORCE_MAX];
	int rem[USB_DEVICE_UGEN_FORCE_MAX];
	int end;
};

#define USB_PATH "/dev/usb0"

extern char *__progname;

void
usage(void)
{
	fprintf(stderr, "usage: %s [-f dev] [-r vid:pid] [-s vid:pid]\n",
		__progname);
	exit(EXIT_FAILURE);
}

void
usb_list_ioctl(int f, struct usb_device_ugen_force *udf, int set)
{
	int e;

	udf->udf_set = set;
	e = ioctl(f, USB_DEVICE_FORCE_UGEN, udf);
	if (e)
		err(EXIT_FAILURE, "ioctl()");
}

unsigned short
usb_parse_id(char *usb_id, int dev)
{
	unsigned short id;
	char *pid;

	id = 0;
  
	if (!dev)
		id = strtol(usb_id, NULL, 0);
	else {
		pid = strchr(usb_id, ':');
		if (!pid)
		errx(EXIT_FAILURE, "can't parse usb vendor_id:device_id '%s'",
			usb_id);
		id = strtol(pid + 1, NULL, 0);
	}

	if (!id)
		errx(EXIT_FAILURE, "vid/pid can't be zero '%s'", usb_id);
  
	return id;
}

void
usb_list_mod(struct usb_change_list *u, char *usb_id, int i, int rem)
{
	unsigned short vid, pid;
  
	if (i >= USB_DEVICE_UGEN_FORCE_MAX)
		errx(EXIT_FAILURE, "too many changes, max changes is %d",
			USB_DEVICE_UGEN_FORCE_MAX);

	vid = usb_parse_id(usb_id, 0);
	pid = usb_parse_id(usb_id, 1);

	u->vid[i] = vid;
	u->pid[i] = pid;
	u->rem[i] = rem;
	u->end = i;
}

void
usb_list_change(struct usb_device_ugen_force *udf, struct usb_change_list *u)
{
	int i, j;

	for (i = 0; i < u->end + 1; i++)
		for (j = 0; j < USB_DEVICE_UGEN_FORCE_MAX; j++)
			if (u->rem[i]) {
				if ((u->vid[i] == udf->udf_ven_id[j]) &&
					(u->pid[i] == udf->udf_dev_id[j])) {
					udf->udf_ven_id[j] = 0;
					udf->udf_dev_id[j] = 0;
				}
			} else {
				if ((!udf->udf_ven_id[j]) &&
					(!udf->udf_dev_id[j])) {
					udf->udf_ven_id[j] = u->vid[i];
					udf->udf_dev_id[j] = u->pid[i];
					break;
				}
			}
}

int
main(int argc, char **argv)
{
	struct usb_device_ugen_force udf;
	struct usb_change_list usb_change;
	int ch, f, i;
	char *dev;

	bzero(&udf, sizeof(udf));
	bzero(&usb_change, sizeof(usb_change));
  
	dev = strdup(USB_PATH);
	if (!dev)
		err(EXIT_FAILURE, "strdup()");

	i = 0;
	while ((ch = getopt(argc, argv, "f:r:s:")) != -1) {
		switch (ch) {
		case 'f':
			free(dev);
			dev = optarg;
			break;
		case 'r':
			udf.udf_set = 1;
			usb_list_mod(&usb_change, optarg, i, 1);
			i++;
			break;
		case 's':
			udf.udf_set = 1;
			usb_list_mod(&usb_change, optarg, i, 0);
			i++;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc)
		usage();
  
	f = open(dev, O_RDONLY);
	if (f == -1)
		err(EXIT_FAILURE, "can't open usb device %s", dev);

	if (udf.udf_set) {
		usb_list_ioctl(f, &udf, 0);
		usb_list_change(&udf, &usb_change);
		usb_list_ioctl(f, &udf, 1);
	}

	usb_list_ioctl(f, &udf, 0);
	printf("currently ugen forced devices:\n");
	for (i = 0; i < USB_DEVICE_UGEN_FORCE_MAX; i++)
		if (udf.udf_ven_id[i] && udf.udf_dev_id[i])
			printf("VID:0x%04x PID:0x%04x\n", udf.udf_ven_id[i],
				udf.udf_dev_id[i]);

	return (EXIT_SUCCESS);
}
