// SPDX-License-Identifier: GPL-2.0
/*
 * Surface Dual: sondeo breve y adaptativo antes de WFI en un invitado arm64 de Hyper-V.
 * Despertar una vCPU dormida cuesta ~80 us por interrupción (medido: 4 KiB O_DSYNC 135 us dormida, 56 us despierta).
 * Reutiliza el estado POLL genérico y el gobernador haltpoll (como cpuidle-haltpoll, que es solo x86+KVM):
 * la ventana crece solo si los despertares llegan dentro de ella y cae a 0 en reposo.
 */
#include <linux/cpu.h>
#include <linux/cpuidle.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched/idle.h>
#include <asm/mshyperv.h>

static int sdpoll_wfi(struct cpuidle_device *dev, struct cpuidle_driver *drv, int index)
{
	if (current_clr_polling_and_test())
		return index;
	arch_cpu_idle();
	return index;
}

static struct cpuidle_driver sdpoll_driver = {
	.name = "sdpoll",
	.governor = "haltpoll",
	.owner = THIS_MODULE,
	.states = {
		{ /* POLL: cpuidle_poll_state_init() */ },
		{
			.enter = sdpoll_wfi,
			.exit_latency = 1,
			.target_residency = 1,
			.power_usage = -1,
			.name = "WFI",
			.desc = "wfi",
		},
	},
	.safe_state_index = 0,
	.state_count = 2,
};

static int __init sdpoll_init(void)
{
	if (!hv_is_hyperv_initialized())
		return -ENODEV;
	cpuidle_poll_state_init(&sdpoll_driver);
	return cpuidle_register(&sdpoll_driver, NULL);
}
device_initcall(sdpoll_init);
