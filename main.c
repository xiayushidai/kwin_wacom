#include <libudev.h>
#include <stdio.h>
#include <stdlib.h>

static struct udev_monitor *udev_monitor;

int config_udev_pre_init(void);
int config_udev_init(void);

void main(void){
    int ret;
    ret=config_udev_pre_init();
    if(ret==0){
        printf("config_udev_pre_init error");    
    }
    config_udev_init();
    if(ret==0){
        printf("config_udev_init error");    
    }
    printf("success\n");
}

int config_udev_pre_init(void){
    struct udev *udev;
    udev = udev_new();
    if (!udev)
      return 0;
    
    udev_monitor = udev_monitor_new_from_netlink(udev, "udev");
    if (!udev_monitor)
      return 0;
    
    udev_monitor_filter_add_match_tag(udev_monitor, "seat0");
    udev_monitor_filter_add_match_subsystem_devtype(udev_monitor, "input",NULL);
    if (udev_monitor_enable_receiving(udev_monitor)) {
        printf("config/udev: failed to bind the udev monitor\n");
        return 0;
    }
    return 1;
}

int config_udev_init(void){
    struct udev *udev;
    struct udev_enumerate *enumerate;
    struct udev_list_entry *devices, *device;

    udev = udev_monitor_get_udev(udev_monitor);
    enumerate = udev_enumerate_new(udev);
    if (!enumerate)
        return 0;

    udev_enumerate_add_match_subsystem(enumerate, "input");

    udev_enumerate_scan_devices(enumerate);
    devices = udev_enumerate_get_list_entry(enumerate);
    udev_list_entry_foreach(device, devices) {
        const char *syspath = udev_list_entry_get_name(device);
        struct udev_device *udev_device =
            udev_device_new_from_syspath(udev, syspath);

        /* Device might be gone by the time we try to open it */
        if (!udev_device)
            continue;

        device_added(udev_device);
        udev_device_unref(udev_device);
    }
    udev_enumerate_unref(enumerate);
    return 1;
    //TODO
}
