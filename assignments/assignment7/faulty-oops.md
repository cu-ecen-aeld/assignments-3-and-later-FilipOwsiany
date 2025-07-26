root@qemuarm64:/#  echo “hello_world” > /dev/faulty
[  364.432297] Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000
[  364.432712] Mem abort info:
[  364.432795]   ESR = 0x0000000096000045
[  364.432911]   EC = 0x25: DABT (current EL), IL = 32 bits
[  364.433048]   SET = 0, FnV = 0
[  364.433135]   EA = 0, S1PTW = 0
[  364.433220]   FSC = 0x05: level 1 translation fault
[  364.433351] Data abort info:
[  364.433444]   ISV = 0, ISS = 0x00000045
[  364.433538]   CM = 0, WnR = 1
[  364.433687] user pgtable: 4k pages, 39-bit VAs, pgdp=0000000043da9000
[  364.433849] [0000000000000000] pgd=0000000000000000, p4d=0000000000000000, pud=0000000000000000
[  364.434256] Internal error: Oops: 0000000096000045 [#1] PREEMPT SMP
[  364.434579] Modules linked in: scull(O) hello(O) faulty(O)
[  364.435146] CPU: 1 PID: 441 Comm: sh Tainted: G           O      5.15.184-yocto-standard #1
[  364.435360] Hardware name: linux,dummy-virt (DT)
[  364.435611] pstate: 80000005 (Nzcv daif -PAN -UAO -TCO -DIT -SSBS BTYPE=--)
[  364.435810] pc : faulty_write+0x18/0x20 [faulty]
[  364.436214] lr : vfs_write+0xf8/0x2a0
[  364.436323] sp : ffffffc00f94bd80
[  364.436418] x29: ffffffc00f94bd80 x28: ffffff800205d280 x27: 0000000000000000
[  364.436667] x26: 0000000000000000 x25: 0000000000000000 x24: 0000000000000000
[  364.436849] x23: 0000000000000000 x22: ffffffc00f94bdc0 x21: 00000055778d63c0
[  364.437033] x20: ffffff80036e9000 x19: 0000000000000012 x18: 0000000000000000
[  364.437209] x17: 0000000000000000 x16: 0000000000000000 x15: 0000000000000000
[  364.437393] x14: 0000000000000000 x13: 0000000000000000 x12: 0000000000000000
[  364.437569] x11: 0000000000000000 x10: 0000000000000000 x9 : ffffffc008271648
[  364.437772] x8 : 0000000000000000 x7 : 0000000000000000 x6 : 0000000000000000
[  364.437950] x5 : 0000000000000001 x4 : ffffffc000b90000 x3 : ffffffc00f94bdc0
[  364.438129] x2 : 0000000000000012 x1 : 0000000000000000 x0 : 0000000000000000
[  364.438416] Call trace:
[  364.438506]  faulty_write+0x18/0x20 [faulty]
[  364.438639]  ksys_write+0x74/0x110
[  364.438730]  __arm64_sys_write+0x24/0x30
[  364.439066]  invoke_syscall+0x5c/0x130
[  364.439239]  el0_svc_common.constprop.0+0x4c/0x100
[  364.439391]  do_el0_svc+0x4c/0xc0
[  364.439490]  el0_svc+0x28/0x80
[  364.439599]  el0t_64_sync_handler+0xa4/0x130
[  364.439718]  el0t_64_sync+0x1a0/0x1a4
[  364.439973] Code: d2800001 d2800000 d503233f d50323bf (b900003f) 
[  364.440392] ---[ end trace b96b4741ceb35c8e ]---
Segmentation fault

Poky (Yocto Project Reference Distro) 4.0.28 qemuarm64 /dev/ttyAMA0

qemuarm64 login: 

The kernel crashed due to a NULL pointer dereference in the faulty_write function from the faulty kernel module. When the command echo "hello_world" > /dev/faulty was executed, the write operation triggered faulty_write(), which deliberately dereferenced address 0 (*(int*)0 = 0;). This caused a data abort at virtual address 0x0000000000000000, leading to an "Oops" and a segmentation fault in user space. The crash trace confirms the fault occurred at faulty_write+0x18/0x20.