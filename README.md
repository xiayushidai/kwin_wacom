# kwin_wacom


X_ListInputDevices
X_SetDeviceMode
ProcXSetDeviceMode

/home/test/Xorg -nolisten tcp -auth /var/run/sddm/{4c96aef9-ac3e-469c-9d82-b690c9f67eea} -background none -noreset -displayfd 17 -seat seat0 vt1

stubmain.c
main()
  dix_main()      //程序start
    InitOutput()
      config_pre_init()
        config_udev_pre_init //初始化udev的监视器。
    InitInput
      config_init()
        config_udev_init()  //通过udev获取设备。
          udev = udev_monitor_get_udev(udev_monitor);
          enumerate = udev_enumerate_new(udev);
          udev_enumerate_add_match_subsystem(enumerate, "input");
          udev_enumerate_scan_devices(enumerate);
          device_added(udev_device)
            NewInputDeviceRequest
              xf86NewInputDevice
                调用PreInit 驱动接口。
                xf86ActivateDevice
                  AddInputDevice
                    XIRegisterPropertyHandler

dev->public.devicePrivate

InputInfoPtr

自己编译的Xorg，log会输出到/usr/local/var/log/xxx.log下面，
如果没有该目录，Xorg启动失败，且没有其他提示信息，比较难定位。

mv /usr/local/lib/xorg  /usr/local/lib/xorg.org
1.ln -svf  /usr/lib/xorg  /usr/local/lib/xorg
2.ln -svf /usr/bin/xkb* /usr/local/bin/
3.sudo mkdir -p /usr/local/share/X11
  sudo cp -ar /usr/share/X11/* /usr/local/share/X11/


用自己编译的xorg进行调试时，系统需要的驱动路径，日志路径，conf路径，都加了local。
需要将所需文件拷贝到相应位置。


xf86Xinput.c:
  NewInputDeviceRequest  //定义

xf86AutoConfig
xf86HandleConfigFile
sysdirname=/usr/share/X11/xorg.conf.d

1.检查驱动怎样加载的。
  loadmod.c
    LoadModule
      LoaderOpen()
        ret = dlopen(module, RTLD_LAZY | RTLD_GLOBAL) //打开wacom_drv.so。
      LoaderSymbolFromModule
        dlsym(handle, name)
2.确认到底在哪边进行的set.
   已经确认，在wacom_drv.so
3.自己编的wacom_drv.so是否可用。
  可用的话，测试。
    wcmDevSwitchMode
      wcmDevSwitchModeCall
        set_absolute(pInfo,TRUE);

4.查看debian是怎样在wayland下实现设置的。
  虚拟机下识别不了设备，暂时不能调查。
5.wcmPreInit什么时候执行。
  xf86NewInputDevice 中调用。
    rval = drv->PreInit(drv, pInfo, 0);

6.查看pInfo的初始化。
  dixLookupDevice
    从inputInfo.devices中获取。
  SetDeviceMode
    (InputInfoPtr)pInfo=dev->public.devicePrivate
    (*pInfo->switch_mode)(client,dev,mode);
      pInfo=dev->public.devicePrivate
        priv=pInfo->private
        (WacomDevicePtr)priv->flag
  基本是在WcmPreInit中初始化的。

7.怎样将所有设备放到数组中。
  inputInfo.devices

8.设备支持热插拔。
