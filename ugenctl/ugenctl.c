#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <err.h>
#include <dev/usb/usb.h>

struct usb_dev_list {
        u_short vid;
        u_short pid;
        u_char addr;
        char *ctrl;
        struct usb_dev_list *next;
};

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

	udf->udf_cmd = set;
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

struct usb_dev_list *
usb_dev_get(void)
{
        struct usb_device_info inf;
	struct usb_dev_list *ret;
        struct usb_dev_list **p;
        char buf[11];
	int a, f, i;

	ret = NULL;
	p = &ret;
	for (i = 0; i < 10; i++) {
	        snprintf(buf, sizeof(buf), "/dev/usb%d", i);
		f = open(buf, O_RDONLY);
		if (f == -1)
		        continue;
		for (a = 1; a < USB_MAX_DEVICES; a++) {
		        inf.udi_addr = a;
			if (ioctl(f, USB_DEVICEINFO, &inf))
			        continue;
			*p = malloc(sizeof(**p));
			if (!*p)
			        err(EXIT_FAILURE, "malloc()");
			(*p)->addr = a;
			(*p)->ctrl = strdup(buf);
			if (!((*p)->ctrl))
              		        err(EXIT_FAILURE, "malloc()");
			(*p)->vid = inf.udi_vendorNo;
			(*p)->pid = inf.udi_productNo;
			(*p)->next = NULL;
			p = &((*p)->next);
		}
		close(f);
	}
	return (ret);
}

void
usb_dev_reset(struct usb_change_list *c, struct usb_dev_list *l)
{
  	struct usb_device_ugen_force udf;
	struct usb_dev_list *p;
	int f, i;

       	for (i = 0; i < c->end + 1; i++)
	        for (p = l; p; p = p->next)
		        if ((c->vid[i] == p->vid) && (c->pid[i] == p->pid)) {
			        f = open(p->ctrl, O_RDONLY);
				if (f == -1)
				        err(EXIT_FAILURE,
					    "can't open usb device %s",
					    p->ctrl);
				udf.udf_addr = p->addr;
				usb_list_ioctl(f, &udf,
					       USB_DEVICE_UGEN_FORCE_RESET);
				close(f);
			}
}

int
main(int argc, char **argv)
{
	struct usb_device_ugen_force udf;
	struct usb_change_list usb_change;
	struct usb_dev_list *list;
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
			udf.udf_cmd = USB_DEVICE_UGEN_FORCE_SET;
			usb_list_mod(&usb_change, optarg, i, 1);
			i++;
			break;
		case 's':
		        udf.udf_cmd = USB_DEVICE_UGEN_FORCE_SET;
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

	if (udf.udf_cmd == USB_DEVICE_UGEN_FORCE_SET) {
                list = usb_dev_get();
		usb_list_ioctl(f, &udf, USB_DEVICE_UGEN_FORCE_GET);
		usb_list_change(&udf, &usb_change);
		usb_list_ioctl(f, &udf, USB_DEVICE_UGEN_FORCE_SET);
		usb_dev_reset(&usb_change, list);
	}

	usb_list_ioctl(f, &udf, USB_DEVICE_UGEN_FORCE_GET);
	printf("currently ugen forced devices:\n");
	for (i = 0; i < USB_DEVICE_UGEN_FORCE_MAX; i++)
		if (udf.udf_ven_id[i] && udf.udf_dev_id[i])
			printf("VID:0x%04x PID:0x%04x\n", udf.udf_ven_id[i],
				udf.udf_dev_id[i]);

	return (EXIT_SUCCESS);
}
