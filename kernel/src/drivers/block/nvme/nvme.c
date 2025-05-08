#include "drivers/bus/pci.h"
#include "drivers/block/nvme/nvme.h"
#include "kprint.h"

NVME_NAMESPACE *namespaces[MAX_NVME_DEV_NUM];
uint64_t nvme_device_num = 0;

void nvme_init()
{
    pci_device_t *device = pci_find_class(0x010802);
    if (!device)
    {
        kerror("No NVME controller found");
        return;
    }

    nvme_driver_init(device->bars[0].address, device->bars[0].size);
}
