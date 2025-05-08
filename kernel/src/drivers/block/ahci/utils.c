#include <io.h>
#include <kprint.h>
#include "drivers/block/ahci/ahci.h"

int ahci_try_send(struct hba_port *port, int slot)
{
    int retries = 0, bitmask = 1 << slot;

    // 确保端口是空闲的
    wait_until(!(port->regs[HBA_RPxTFD] & (HBA_PxTFD_BSY | HBA_PxTFD_DRQ)));

    hba_clear_reg(port->regs[HBA_RPxIS]);

    while (retries < MAX_RETRY)
    {
        // PxCI寄存器置位，告诉HBA这儿有个数据需要发送到SATA端口
        port->regs[HBA_RPxCI] = bitmask;

        wait_until(!(port->regs[HBA_RPxCI] & bitmask));

        port->regs[HBA_RPxCI] &= ~bitmask; // ensure CI bit is cleared
        if ((port->regs[HBA_RPxTFD] & HBA_PxTFD_ERR))
        {
            // 有错误
            sata_read_error(port);
            retries++;
        }
        else
        {
            break;
        }
    }

    hba_clear_reg(port->regs[HBA_RPxIS]);

    return retries < MAX_RETRY;
}

// ATA 状态寄存器 (STS) 位定义
#define ATA_STS_ERR (1 << 0) // Error
#define ATA_STS_DRQ (1 << 3) // Data Request
#define ATA_STS_BSY (1 << 7) // Busy

// ATA 错误寄存器 (ERR) 位定义
#define ATA_ERR_AMNF (1 << 0)  // Address Mark Not Found
#define ATA_ERR_TKZNF (1 << 1) // Track Zero Not Found
#define ATA_ERR_ABRT (1 << 2)  // Command Aborted
#define ATA_ERR_IDNF (1 << 4)  // ID Not Found (LBA out of range)
#define ATA_ERR_UNC (1 << 6)   // Uncorrectable Data Error

// AHCI 命令寄存器 (PxCMD) 位定义
#define AHCI_CMD_FR (1 << 14) // FIS Receive Running
#define AHCI_CMD_CR (1 << 15) // Command Running

void ahci_post(struct hba_port *port, struct hba_cmd_state *state, int slot)
{
    int bitmask = 1 << slot;

    // 确保端口是空闲的
    uint64_t counter = wait_until_expire(!(port->regs[HBA_RPxTFD] & (HBA_PxTFD_BSY | HBA_PxTFD_DRQ)), 10000000000);
    if (!counter)
    {
        kerror("AHCI wait timeout");
        return;
    }

    hba_clear_reg(port->regs[HBA_RPxIS]);

    port->cmdctx.issued[slot] = state;
    port->cmdctx.tracked_ci |= bitmask;
    port->regs[HBA_RPxCI] |= bitmask;

    while (1)
    {
        if ((port->regs[HBA_RPxCI] & (1 << slot)) == 0)
            break;
        if (port->regs[HBA_RPxIS] & HBA_PxINTR_TFE)
        {
            goto err;
        }
    }

    if (port->regs[HBA_RPxIS] & HBA_PxINTR_TFE)
    {
    err:
        kdebug("AHCI task file error");

        // 1. 读取 PxSERR（Serial ATA Error Register）
        uint32_t serr = port->regs[HBA_RPxSERR];
        if (serr)
        {
            kdebug("[AHCI] SERR Error: 0x%x", serr);
            if (serr & (1 << 16))
                kdebug("  - FIS Exchange Error");
            if (serr & (1 << 17))
                kdebug("  - FIS CRC Error");
            if (serr & (1 << 18))
                kdebug("  - FIS Transmission Error");
            if (serr & (1 << 19))
                kdebug("  - Command Protocol Error");
            // 清除错误标志
            port->regs[HBA_RPxSERR] = serr;
        }

        // 2. 读取 PxTFD（Task File Data）
        uint32_t tfd = port->regs[HBA_RPxTFD];
        uint8_t sts = tfd & 0xFF;        // Status (STS)
        uint8_t err = (tfd >> 8) & 0xFF; // Error (ERR)

        if (sts & ATA_STS_ERR)
        {
            kdebug("[AHCI] ATA Error: STS=0x%x, ERR=0x%x", sts, err);
            if (err & ATA_ERR_ABRT)
                kdebug("  - Command Aborted");
            if (err & ATA_ERR_IDNF)
                kdebug("  - LBA Out of Range");
            if (err & ATA_ERR_UNC)
                kdebug("  - Uncorrectable Data");
            if (err & ATA_ERR_AMNF)
                kdebug("  - Address Mark Not Foundn");
        }

        // 3. 检查 PxCMD（Command and Status）
        uint32_t cmd = port->regs[HBA_RPxCMD];
        if (cmd & AHCI_CMD_CR)
        {
            kdebug("[AHCI] Command Running (CR) stuck!");
        }
        if (cmd & AHCI_CMD_FR)
        {
            kdebug("[AHCI] FIS Receive (FR) stuck!");
        }

        // 4. 检查 PxCI（Command Issue）
        uint32_t ci = port->regs[HBA_RPxCI];
        if (ci)
        {
            kdebug("[AHCI] Pending Commands (CI): 0x%x", ci);
            for (int slot = 0; slot < 32; slot++)
            {
                if (ci & (1 << slot))
                {
                    kdebug("  - Command Slot %d not completed", slot);
                }
            }
        }
    }

    port->regs[HBA_RPxCI] &= ~bitmask;

    hba_clear_reg(port->regs[HBA_RPxIS]);
}
