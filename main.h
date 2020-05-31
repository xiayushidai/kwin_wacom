#ifndef X_MAIN
#define X_MAIN
#include <stdio.h>
#include <xorg/input.h>
#include <xorg/list.h>
#include <xorg/optionstr.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <xorg/Xprintf.h>
#include <xorg/xf86.h>
#include <xorg/xf86Xinput.h>
#include <stdarg.h>
#include <string.h>
#include <X11/X.h>
#include <libudev.h>
#include <stdlib.h>
#include <errno.h>
#include <regex.h>
#include <dlfcn.h>

static void input_option_free(InputOption *o);
void input_option_free_list(InputOption **opt);
InputOption * input_option_new(InputOption *list, const char *key, const char *value);
int NewInputDeviceRequest(InputOption *options, InputAttributes * attrs,DeviceIntPtr *pdev);
int xf86NewInputDevices(InputInfoPtr pInfo,DeviceIntPtr *pdev,BOOL is_auto);
#define UDEV_XKB_PROP_KEY "xkb"

InputDriverPtr *xf86InputDriverList = NULL;
int xf86NumInputDrivers = 0;


void *
LoaderOpen(const char *module, int *errmaj, int *errmin)
{
    void *ret;

    printf("Loading %s\n", module);

    if (!(ret = dlopen(module, RTLD_LAZY | RTLD_GLOBAL))) {
        printf("Failed to load %s: %s\n", module, dlerror());
        if (errmaj)
            *errmaj = LDR_NOLOAD;
        if (errmin)
            *errmin = LDR_NOLOAD;
        return NULL;
    }
    return ret;
}

void * LoaderSymbolFromModule(void *handle, const char *name)
{
    return dlsym(handle, name);
}

typedef struct module_desc {
    struct module_desc *child;
    struct module_desc *sib;
    struct module_desc *parent;
    char *name;
    char *path;
    void *handle;
    ModuleSetupProc SetupProc;
    ModuleTearDownProc TearDownProc;
    void *TearDownData;         /* returned from SetupProc */
    const XF86ModuleVersionInfo *VersionInfo;
} ModuleDesc, *ModuleDescPtr;

ModuleDescPtr LoadModule(const char *module, const char *path, const char **subdirlist,
           const char **patternlist, void *options,
           const XF86ModReqInfo * modreq, int *errmaj, int *errmin)
{
    XF86ModuleData *initdata = NULL;
    char **pathlist = NULL;
    char *found = "/usr/lib/xorg/modules/input/wacom_drv.so";
    char *name = NULL;
    char **path_elem = NULL;
    char *p = NULL;
    ModuleDescPtr ret = NULL;
    int noncanonical = 0;
    char *m = NULL;
    const char **cim;

    printf("LoadModule: %s\n", module);


    ret->handle = LoaderOpen(found, errmaj, errmin);
    if (ret->handle == NULL)
        goto LoadModule_fail;
    ret->path = strdup(found);

    /* drop any explicit suffix from the module name */
    p = strchr(name, '.');
    if (p)
        *p = '\0';

    /*
     * now check if the special data object <modulename>ModuleData is
     * present.
     */
    if (asprintf(&p, "%sModuleData", name) == -1) {
        p = NULL;
        if (errmaj)
            *errmaj = LDR_NOMEM;
        if (errmin)
            *errmin = 0;
        goto LoadModule_fail;
    }
    initdata = LoaderSymbolFromModule(ret->handle, p);
    if (initdata) {
        ModuleSetupProc setup;
        ModuleTearDownProc teardown;
        XF86ModuleVersionInfo *vers;

        vers = initdata->vers;
        setup = initdata->setup;
        teardown = initdata->teardown;

        if (setup)
            ret->SetupProc = setup;
        if (teardown)
            ret->TearDownProc = teardown;
        ret->VersionInfo = vers;
    }
    else {
        /* no initdata, fail the load */
        printf("LoadModule: Module %s does not have a %s\n", module, p);
        if (errmaj)
            *errmaj = LDR_INVALID;
        if (errmin)
            *errmin = 0;
        goto LoadModule_fail;
    }
    if (ret->SetupProc) {
        ret->TearDownData = ret->SetupProc(ret, options, errmaj, errmin);  //调用了wacom_drv.so 中的wcmPlug函数，wcmPlug调用了xf86AddInputDriver
        if (!ret->TearDownData) {
            goto LoadModule_fail;
        }
    }
    else if (options) {
        printf("Module Options present, but no SetupProc available for %s\n", module);
    }
    goto LoadModule_fail;

 LoadModule_fail:
    ret=NULL;

    return ret;
}


char *xf86NormalizeName(const char *s)
{
    char *ret, *q;
    const char *p;

    if (s == NULL)
        return NULL;

    ret = malloc(strlen(s) + 1);
    for (p = s, q = ret; *p != 0; p++) {
        switch (*p) {
        case '_':
        case ' ':
        case '\t':
            continue;
        default:
            if (isupper(*p))
                *q++ = tolower(*p);
            else
                *q++ = *p;
        }
    }
    *q = '\0';
    return ret;
}

void *xf86LoadOneModule(const char *name, void *opt)
{
    int errmaj, errmin;
    char *Name;
    void *mod;

    if (!name)
        return NULL;

    /* Normalise the module name */
    Name = xf86NormalizeName(name);

    /* Skip empty names */
    if (Name == NULL)
        return NULL;
    if (*Name == '\0') {
        free(Name);
        return NULL;
    }

    mod = LoadModule(Name, NULL, NULL, NULL, opt, NULL, &errmaj, &errmin);
    if (!mod)
        printf("loadOneModule: %s %d %d\n", Name, errmaj, errmin);
    free(Name);
    return mod;
}



void xf86AddInputDriver(InputDriverPtr driver, void *module, int flags)  //没有找到调用的地方,这个是在libinput_drv.so里面调用的，这你敢信？ open之后
{
    /* Don't add null entries */
    if (!driver)
        return;

    if (xf86InputDriverList == NULL)
        xf86NumInputDrivers = 0;

    xf86NumInputDrivers++;
    xf86InputDriverList = xnfreallocarray(xf86InputDriverList,
                                          xf86NumInputDrivers,
                                          sizeof(InputDriverPtr));
    xf86InputDriverList[xf86NumInputDrivers - 1] =
        xnfalloc(sizeof(InputDriverRec));
    *xf86InputDriverList[xf86NumInputDrivers - 1] = *driver;
    xf86InputDriverList[xf86NumInputDrivers - 1]->module = module;
}

void xf86DeleteInputDriver(int drvIndex)
{
    if (xf86InputDriverList[drvIndex] && xf86InputDriverList[drvIndex]->module)
        //UnloadModule(xf86InputDriverList[drvIndex]->module);
    free(xf86InputDriverList[drvIndex]);
    xf86InputDriverList[drvIndex] = NULL;
}

int
xf86nameCompare(const char *s1, const char *s2)
{
    char c1, c2;

    if (!s1 || *s1 == 0) {
        if (!s2 || *s2 == 0)
            return 0;
        else
            return 1;
    } else if (!s2 || *s2 == 0) {
        return -1;
    }

    while (*s1 == '_' || *s1 == ' ' || *s1 == '\t')
        s1++;
    while (*s2 == '_' || *s2 == ' ' || *s2 == '\t')
        s2++;
    c1 = (isupper(*s1) ? tolower(*s1) : *s1);
    c2 = (isupper(*s2) ? tolower(*s2) : *s2);
    while (c1 == c2) {
        if (c1 == '\0')
            return 0;
        s1++;
        s2++;
        while (*s1 == '_' || *s1 == ' ' || *s1 == '\t')
            s1++;
        while (*s2 == '_' || *s2 == ' ' || *s2 == '\t')
            s2++;
        c1 = (isupper(*s1) ? tolower(*s1) : *s1);
        c2 = (isupper(*s2) ? tolower(*s2) : *s2);
    }
    return c1 - c2;
}

int
xf86NameCmp(const char *s1, const char *s2)
{
    return xf86nameCompare(s1, s2);
}

InputDriverPtr xf86LookupInputDriver(const char *name)
{
    int i;
    for (i = 0; i < xf86NumInputDrivers; i++) {
        printf("xf86NumInputDrivers===%s\n",xf86InputDriverList[i]->driverName);
        if (xf86InputDriverList[i] && xf86InputDriverList[i]->driverName &&
            xf86NameCmp(name, xf86InputDriverList[i]->driverName) == 0)
            return xf86InputDriverList[i]; //输入设备驱动列表，这个在哪里初始化？
    }
    return NULL;
}

void *XNFreallocarray(void *ptr, size_t nmemb, size_t size)
{
    void *ret = reallocarray(ptr, nmemb, size);

    if (!ret)
        printf("XNFreallocarray: Out of memory");
    return ret;
}

void *
XNFalloc(unsigned long n)
{
    void *r;

    r = malloc(n);
    if (!r) {
        perror("malloc failed");
        exit(1);
    }
    return r;
}

static inline InputDriverPtr
xf86LoadInputDriver(const char *driver_name)
{
    InputDriverPtr drv = NULL;

    /* Memory leak for every attached device if we don't
     * test if the module is already loaded first */
    drv = xf86LookupInputDriver(driver_name);
    if (!drv) {
        if (xf86LoadOneModule(driver_name, NULL))
            drv = xf86LookupInputDriver(driver_name);
    }

    return drv;
}

XF86OptionPtr xf86findOption(XF86OptionPtr list, const char *name)
{
    while (list) {
        if (xf86nameCompare(list->opt_name, name) == 0)
            return list;
        list = list->list.next;
    }
    return NULL;
}

GenericListPtr xf86addListItem(GenericListPtr head, GenericListPtr new)
{
    GenericListPtr p = head;
    GenericListPtr last = NULL;

    while (p) {
        last = p;
        p = p->next;
    }

    if (last) {
        last->next = new;
        return head;
    }
    else
        return new;
}

static XF86OptionPtr addNewOption2(XF86OptionPtr head, char *name, char *_val, int used)
{
    XF86OptionPtr new, old = NULL;

    /* Don't allow duplicates, free old strings */
    if (head != NULL && (old = xf86findOption(head, name)) != NULL) {
        new = old;
        free(new->opt_name);
        free(new->opt_val);
    }
    else
        new = calloc(1, sizeof(*new));
    new->opt_name = name;
    new->opt_val = _val;
    new->opt_used = used;

    if (old)
        return head;
    return ((XF86OptionPtr) xf86addListItem((glp) head, (glp) new));
 
}

XF86OptionPtr xf86addNewOption(XF86OptionPtr head, char *name, char *_val)
{
    return addNewOption2(head, name, _val, 0);
}

XF86OptionPtr xf86AddNewOption(XF86OptionPtr head, const char *name, const char *val)
{
    /* XXX These should actually be allocated in the parser library. */
    char *tmp = val ? strdup(val) : NULL;
    char *tmp_name = strdup(name);

    return xf86addNewOption(head, tmp_name, tmp);
}





static char itoa_buf[16];
static const char *itoa(int i)
{
    snprintf(itoa_buf, sizeof(itoa_buf), "%d", i);
    return itoa_buf;
}

int Xasprintf(char **ret, const char *_X_RESTRICT_KYWD format, ...)
{
    int size;
    va_list va;

    va_start(va, format);
    size = vasprintf(ret, format, va);
    va_end(va);
    return size;
}

int
Xvasprintf(char **ret, const char *_X_RESTRICT_KYWD format, va_list va)
{
#ifdef HAVE_VASPRINTF
    return vasprintf(ret, format, va);
#else
    int size;
    va_list va2;

    va_copy(va2, va);
    size = vsnprintf(NULL, 0, format, va2);
    va_end(va2);

    *ret = malloc(size + 1);
    if (*ret == NULL)
        return -1;

    vsnprintf(*ret, size + 1, format, va);
    (*ret)[size] = 0;
    return size;
#endif
}

int xstrncasecmp(const char *s1, const char *s2, size_t n)
{
    if (n != 0) {
        const u_char *us1 = (const u_char *) s1, *us2 = (const u_char *) s2;

        do {
            if (tolower(*us1) != tolower(*us2++))
                return (tolower(*us1) - tolower(*--us2));
            if (*us1++ == '\0')
                break;
        } while (--n != 0);
    }

    return 0;
}

char **
xstrtokenize(const char *str, const char *separators)
{
    char **list, **nlist;
    char *tok, *tmp;
    unsigned num = 0, n;

    if (!str)
        return NULL;
    list = calloc(1, sizeof(*list));
    if (!list)
        return NULL;
    tmp = strdup(str);
    if (!tmp)
        goto error;
    for (tok = strtok(tmp, separators); tok; tok = strtok(NULL, separators)) {
        nlist = reallocarray(list, num + 2, sizeof(*list));
        if (!nlist)
            goto error;
        list = nlist;
        list[num] = strdup(tok);
        if (!list[num])
            goto error;
        list[++num] = NULL;
    }
    free(tmp);
    return list;

 error:
    free(tmp);
    for (n = 0; n < num; n++)
        free(list[n]);
    free(list);
    return NULL;
}

void
input_option_free_list(InputOption **opt)
{
    InputOption *element, *tmp;

    nt_list_for_each_entry_safe(element, tmp, *opt, list.next) {
        nt_list_del(element, *opt, InputOption, list.next);

        input_option_free(element);
    }
    *opt = NULL;
}

static void
input_option_free(InputOption *o)
{
    free(o->opt_name);
    free(o->opt_val);
    free(o->opt_comment);
    free(o);
}

void input_option_set_key(InputOption *opt, const char *key)
{
    free(opt->opt_name);
    if (key)
        opt->opt_name = strdup(key);
}

void input_option_set_value(InputOption *opt, const char *value)
{
    free(opt->opt_val);
    if (value)
        opt->opt_val = strdup(value);
}

const char * input_option_get_key(const InputOption *opt)
{
    return opt->opt_name;
}

const char *input_option_get_value(const InputOption *opt)
{
    return opt->opt_val;
}

InputOption * input_option_new(InputOption *list, const char *key, const char *value)
{
    InputOption *opt = NULL;

    if (!key)
        return NULL;

    if (list) {
        nt_list_for_each_entry(opt, list, list.next) {
            if (strcmp(input_option_get_key(opt), key) == 0) {
                input_option_set_value(opt, value);
                return list;
            }
        }
    }

    opt = calloc(1, sizeof(InputOption));
    if (!opt)
        return NULL;

    nt_list_init(opt, list.next);
    input_option_set_key(opt, key);
    input_option_set_value(opt, value);

    if (list) {
        nt_list_append(opt, list, InputOption, list.next);

        return list;
    }
    else
        return opt;
}

InputInfoPtr xf86AllocateInput(void)
{
    InputInfoPtr pInfo;

    pInfo = calloc(sizeof(*pInfo), 1);
    if (!pInfo)
        return NULL;

    pInfo->fd = -1;
    pInfo->type_name = "UNKNOWN";

    return pInfo;
}

int
xstrcasecmp(const char *str1, const char *str2)
{
    const u_char *us1 = (const u_char *) str1, *us2 = (const u_char *) str2;

    while (tolower(*us1) == tolower(*us2)) {
        if (*us1++ == '\0')
            return 0;
        us2++;
    }

    return (tolower(*us1) - tolower(*us2));
}

char *Xstrdup(const char *s)
{
    if (s == NULL)
        return NULL;
    return strdup(s);
}

#define MUL_NO_OVERFLOW	((size_t)1 << (sizeof(size_t) * 4))

void *reallocarray(void *optr, size_t nmemb, size_t size)
{
	if ((nmemb >= MUL_NO_OVERFLOW || size >= MUL_NO_OVERFLOW) &&
	    nmemb > 0 && SIZE_MAX / nmemb < size) {
		errno = ENOMEM;
		return NULL;
	}
	return realloc(optr, size * nmemb);
}



#endif