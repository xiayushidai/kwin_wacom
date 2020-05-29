#include "main.h"

void device_added(struct udev_device *udev_device){
    const char *path, *name = NULL;
    const char *syspath;
    const char *tags_prop;
    const char *key, *value, *tmp;
    char *config_info = NULL;
    InputAttributes attrs = { };
    struct udev_list_entry *set, *entry;
    DeviceIntPtr dev = NULL;


    InputOption *input_options;
    struct udev_device *parent;
    dev_t devnum;
    int rc;

    path = udev_device_get_devnode(udev_device);

    syspath = udev_device_get_syspath(udev_device);
    printf("path==%s\n",path);
    if (!path || !syspath)
        return;
    devnum = udev_device_get_devnum(udev_device);

    value = udev_device_get_property_value(udev_device, "ID_INPUT"); //通过udev获取value。
    if (value && !strcmp(value, "0")) {
        printf("config/udev: ignoring device %s without\n",path);
        return;
    }

    input_options = input_option_new(NULL, "_source", "server/udev");  //初始化input的选项
    if (!input_options)
        return;
    
    parent = udev_device_get_parent(udev_device); //获取设备的父设备。
    if (parent) {
        const char *ppath = udev_device_get_devnode(parent);
        const char *product = udev_device_get_property_value(parent, "PRODUCT");
        const char *pnp_id = udev_device_get_sysattr_value(parent, "id");
        unsigned int usb_vendor, usb_model;

        name = udev_device_get_sysattr_value(parent, "name");
        if (!name) {
            name = udev_device_get_property_value(parent, "NAME");
        }

        /* construct USB ID in lowercase hex - "0000:ffff" */
        if (product &&
            sscanf(product, "%*x/%4x/%4x/%*x", &usb_vendor, &usb_model) == 2) {
            char *usb_id;
            if (asprintf(&usb_id, "%04x:%04x", usb_vendor, usb_model)
                == -1)
                usb_id = NULL;
            else{
                //printf("ppath==%s  product==%s\n",ppath,product);
            }
            attrs.usb_id = usb_id;
        }

        while (!pnp_id && (parent = udev_device_get_parent(parent))) {
            pnp_id = udev_device_get_sysattr_value(parent, "id");
            if (!pnp_id)
                continue;
            attrs.pnp_id = strdup(pnp_id);
            ppath = udev_device_get_devnode(parent);
        }

    }
    if (!name)
        name = "(unnamed)";
    else
        attrs.product = strdup(name);
    
    input_options = input_option_new(input_options, "name", name);
    input_options = input_option_new(input_options, "path", path);
    input_options = input_option_new(input_options, "device", path);
    input_options = input_option_new(input_options, "major", itoa(major(devnum)));
    input_options = input_option_new(input_options, "minor", itoa(minor(devnum)));

    if (path)
        attrs.device = strdup(path);

    tags_prop = udev_device_get_property_value(udev_device, "ID_INPUT.tags");
    attrs.tags = xstrtokenize(tags_prop, ",");

    if (asprintf(&config_info, "udev:%s", syspath) == -1) {
        config_info = NULL;
        goto unwind;
    }

    set = udev_device_get_properties_list_entry(udev_device);

    udev_list_entry_foreach(entry, set) {
        key = udev_list_entry_get_name(entry);
        if (!key)
            continue;
        value = udev_list_entry_get_value(entry);
        if (!strncasecmp(key, UDEV_XKB_PROP_KEY, sizeof(UDEV_XKB_PROP_KEY) - 1)) {
            tmp = key + sizeof(UDEV_XKB_PROP_KEY) - 1;
            if (!strcasecmp(tmp, "rules"))
                input_options =
                    input_option_new(input_options, "xkb_rules", value);
            else if (!strcasecmp(tmp, "layout"))
                input_options =
                    input_option_new(input_options, "xkb_layout", value);
            else if (!strcasecmp(tmp, "variant"))
                input_options =
                    input_option_new(input_options, "xkb_variant", value);
            else if (!strcasecmp(tmp, "model"))
                input_options =
                    input_option_new(input_options, "xkb_model", value);
            else if (!strcasecmp(tmp, "options"))
                input_options =
                    input_option_new(input_options, "xkb_options", value);
        }
        else if (!strcmp(key, "ID_VENDOR")) {
            attrs.vendor = strdup(value);
        } else if (!strncmp(key, "ID_INPUT_", 9)) {
            const struct pfmap {
                const char *property;
                unsigned int flag;
            } map[] = {
                { "ID_INPUT_KEY", ATTR_KEY },
                { "ID_INPUT_KEYBOARD", ATTR_KEYBOARD },
                { "ID_INPUT_MOUSE", ATTR_POINTER },
                { "ID_INPUT_JOYSTICK", ATTR_JOYSTICK },
                { "ID_INPUT_TABLET", ATTR_TABLET },
                { "ID_INPUT_TABLET_PAD", ATTR_TABLET_PAD },
                { "ID_INPUT_TOUCHPAD", ATTR_TOUCHPAD },
                { "ID_INPUT_TOUCHSCREEN", ATTR_TOUCHSCREEN },
                { NULL, 0 },
            };

            /* Anything but the literal string "0" is considered a
             * boolean true. The empty string isn't a thing with udev
             * properties anyway */
            if (value && strcmp(value, "0")) {
                const struct pfmap *m = map;

                while (m->property != NULL) {
                    if (!strcmp(m->property, key)) {
                        attrs.flags |= m->flag;
                    }
                    m++;
                }
            }
        }
    }
    input_options = input_option_new(input_options, "config_info", config_info);
    input_options = input_option_new(input_options, "driver", "wacom");

    rc = NewInputDeviceRequest(input_options,&attrs,&dev);
    if(rc!=0){
        printf("NewInputDeviceRequest error : %d\n",rc);
        return;    
    }

unwind:
    free(config_info);
    input_option_free_list(&input_options);
}


int NewInputDeviceRequest(InputOption *options, InputAttributes * attrs,
                      DeviceIntPtr *pdev)
{
    InputInfoPtr pInfo = NULL;
    InputOption *option = NULL;
    int rval = Success;
    int is_auto = 0;

    pInfo = xf86AllocateInput();
    if (!pInfo)
        return BadAlloc;

    nt_list_for_each_entry(option, options, list.next) {
        const char *key = input_option_get_key(option);
        const char *value = input_option_get_value(option);
        if (strcasecmp(key, "driver") == 0) {
            if (pInfo->driver) {
                rval = BadRequest;
                goto unwind;
            }
            pInfo->driver = xstrdup(value); //将value的值赋给pInfo->driver.
            if (!pInfo->driver) {
                rval = BadAlloc;
                goto unwind;
            }
        }

        if (strcasecmp(key, "name") == 0 || strcasecmp(key, "identifier") == 0) { //忽略大小写的比较
            if (pInfo->name) { //表示已经有值。错误请求。
                rval = BadRequest;
                goto unwind;
            }
            pInfo->name = xstrdup(value);   //赋值
            if (!pInfo->name) {
                rval = BadAlloc;
                goto unwind;
            }
        }

        if (strcmp(key, "_source") == 0 &&
            (strcmp(value, "server/hal") == 0 ||
             strcmp(value, "server/udev") == 0 ||
             strcmp(value, "server/wscons") == 0)) {
            is_auto = 1; //请求为自动，但是当前不能自动添加设备。报错，错误匹配。
        }

        if (strcmp(key, "major") == 0) //主
            pInfo->major = atoi(value);

        if (strcmp(key, "minor") == 0) //从
            pInfo->minor = atoi(value);
    }
    nt_list_for_each_entry(option, options, list.next) {
        /* Copy option key/value strings from the provided list */
        //将option的key value 赋值给pInfo->options
        pInfo->options = xf86AddNewOption(pInfo->options,
                                          input_option_get_key(option),
                                          input_option_get_value(option));
    }
    /* Apply InputClass settings */
    if (attrs) { //如果属性不为0.
        
    }
    printf("pInfo->name==%s\n",pInfo->name);
    if (!pInfo->name) { //没有检出设备。
        printf("No identifier specified, ignoring this device.\n");
        rval = BadRequest;
        goto unwind;
    }
    
    if (!pInfo->driver) { //没有找到设备驱动。
        printf("No input driver specified, ignoring this device.\n");
        rval = BadRequest;
        goto unwind;
    }

    rval = xf86NewInputDevice(pInfo, pdev,TRUE);
unwind:
    return rval;
}

int xf86NewInputDevice(InputInfoPtr pInfo,DeviceIntPtr *pdev,BOOL is_auto)
{
    InputDriverPtr drv = NULL;
    DeviceIntPtr dev = NULL;
    Bool paused;
    int rval;
    char *path = NULL;

    printf("driver======%s\n",pInfo->driver);
    return 0;

    drv = xf86LoadInputDriver(pInfo->driver);
    if (!drv) { //没有合适的驱动。
        printf("No input driver matching `%s'\n", pInfo->driver);
        rval = BadName;
        goto unwind;
    }
    

    //给这个设备使用哪个驱动。
    xf86Msg(X_INFO, "Using input driver '%s' for '%s'\n", drv->driverName,
            pInfo->name);

    if (!drv->PreInit) {//查看驱动是否有PreInit函数。
        printf("Input driver `%s' has no PreInit function (ignoring)\n",drv->driverName);
        rval = BadImplementation;
        goto unwind;
    }
#if 0
    path = xf86CheckStrOption(pInfo->options, "Device", NULL);
    if (path && pInfo->major == 0 && pInfo->minor == 0)
        xf86stat(path, &pInfo->major, &pInfo->minor);

    if (path && (drv->capabilities & XI86_DRV_CAP_SERVER_FD)){
        int fd = systemd_logind_take_fd(pInfo->major, pInfo->minor,
                                        path, &paused);
        if (fd != -1) {
            if (paused) {
                /* Put on new_input_devices list for delayed probe */
                PausedInputDevicePtr new_device = xnfalloc(sizeof *new_device);
                new_device->pInfo = pInfo;

                xorg_list_append(&new_device->node, &new_input_devices_list);
                systemd_logind_release_fd(pInfo->major, pInfo->minor, fd);
                free(path);
                return BadMatch;
            }
            pInfo->fd = fd;
            pInfo->flags |= XI86_SERVER_FD;
            pInfo->options = xf86ReplaceIntOption(pInfo->options, "fd", fd);
        }
    }

    free(path);

    xf86AddInput(drv, pInfo); //pInfo->drv=drv,将pInfo追加到xf86InputDevs末尾

    input_lock();
    rval = drv->PreInit(drv, pInfo, 0); //通过驱动函数，对pInfo进一步初始化。
    input_unlock();

    if (rval != Success) {
        xf86Msg(X_ERROR, "PreInit returned %d for \"%s\"\n", rval, pInfo->name);
        goto unwind;
    }

    if (!(dev = xf86ActivateDevice(pInfo))) { //将设备进行active。
        rval = BadAlloc;
        goto unwind;
    }
#endif
    rval = 0;
unwind:
    return -1;
}