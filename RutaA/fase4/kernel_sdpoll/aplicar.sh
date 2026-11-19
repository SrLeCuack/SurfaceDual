#!/bin/bash
# aplicar.sh <árbol del kernel>: alta idempotente del driver sdpoll (mismo estilo que sdsync/ashmem en kernel_android.sh)
set -e; K=$1; D=$(cd "$(dirname "$0")" && pwd)
cp "$D/cpuidle-sdpoll.c" $K/drivers/cpuidle/cpuidle-sdpoll.c
if ! grep -q '^config SDPOLL_CPUIDLE$' $K/drivers/cpuidle/Kconfig; then
  sed -i '/^config HALTPOLL_CPUIDLE$/i config ARCH_HAS_CPU_RELAX\n\tbool\n\nconfig SDPOLL_CPUIDLE\n\tbool "Surface Dual: halt-poll before WFI on Hyper-V arm64 guests"\n\tdepends on ARM64 \&\& HYPERV\n\tselect ARCH_HAS_CPU_RELAX\n\tselect CPU_IDLE_GOV_HALTPOLL\n' $K/drivers/cpuidle/Kconfig
  sed -i '/^config CPU_IDLE_GOV_HALTPOLL$/,/^$/ s/depends on KVM_GUEST$/depends on KVM_GUEST || (ARM64 \&\& HYPERV)/' $K/drivers/cpuidle/Kconfig
fi
grep -qF 'CONFIG_SDPOLL_CPUIDLE' $K/drivers/cpuidle/Makefile || echo 'obj-$(CONFIG_SDPOLL_CPUIDLE) += cpuidle-sdpoll.o' >> $K/drivers/cpuidle/Makefile
grep -q 'CONFIG_SDPOLL_CPUIDLE' $K/drivers/cpuidle/governors/haltpoll.c || \
  sed -i 's/^\tif (kvm_para_available())$/\tif (kvm_para_available() || IS_ENABLED(CONFIG_SDPOLL_CPUIDLE))/' $K/drivers/cpuidle/governors/haltpoll.c
grep -q 'CONFIG_SDPOLL_CPUIDLE' $K/drivers/cpuidle/governors/haltpoll.c
# Solo se sondea mientras alguna tarea espera E/S: el sondeo general costaba +17 % de un núcleo viendo vídeo (medido 18/11).
grep -q 'nr_iowait' $K/drivers/cpuidle/governors/haltpoll.c || \
  sed -i -e 's/^\tif (dev->poll_limit_ns == 0)$/\tif (dev->poll_limit_ns == 0 || (IS_ENABLED(CONFIG_SDPOLL_CPUIDLE) \&\& !nr_iowait()))/' \
    -e 's/^#include <linux\/kvm_para.h>$/&\n#include <linux\/sched\/stat.h>/' $K/drivers/cpuidle/governors/haltpoll.c
grep -q 'nr_iowait()' $K/drivers/cpuidle/governors/haltpoll.c && grep -q 'sched/stat.h' $K/drivers/cpuidle/governors/haltpoll.c
grep -q 'depends on KVM_GUEST || (ARM64 && HYPERV)' $K/drivers/cpuidle/Kconfig && grep -q 'CONFIG_SDPOLL_CPUIDLE' $K/drivers/cpuidle/Makefile
