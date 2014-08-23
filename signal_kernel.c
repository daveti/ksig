/*
 * signal_kernel.c
 * A kernel module used to send signals to the user space
 * Originated from http://people.ee.ethz.ch/~arkeller/linux/code/signal_kernel_phil_bergeron_burns.c
 * Modified by daveti
 * Aug 23, 2014
 * root@davejingtian.org
 * http://davejingtian.org
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <asm/siginfo.h>	//siginfo
#include <linux/rcupdate.h>	//rcu_read_lock
#include <linux/sched.h>	//find_task_by_pid_type
#include <linux/debugfs.h>
#include <linux/uaccess.h>


#define SIG_TEST 44	// we choose 44 as our signal number (real-time signals are in the range of 33 to 64)

struct dentry *file;

static ssize_t write_pid(struct file *file, const char __user *buf,
                                size_t count, loff_t *ppos)
{
	char mybuf[10];
	int pid = 0;
	int ret;
	struct siginfo info;
	struct task_struct *t;
	/* read the value from user space */
	if(count > 10)
		return -EINVAL;
	copy_from_user(mybuf, buf, count);
	sscanf(mybuf, "%d", &pid);
	pr_info("pid = %d\n", pid);

	/* prepare the signal */
	memset(&info, 0, sizeof(struct siginfo));
	info.si_signo = SIG_TEST;
	/*
	this is bit of a trickery: SI_QUEUE is normally used by sigqueue from user space,
	and kernel space should use SI_KERNEL. But if SI_KERNEL is used the real_time data 
	is not delivered to the user space signal handler function. 
	*/
	info.si_code = SI_QUEUE;
	/* real time signals may have 32 bits of data. */
	info.si_int = 1234;

	rcu_read_lock();
	/* find the task with that pid */
	t = pid_task(find_pid_ns(pid, &init_pid_ns), PIDTYPE_PID);	
	if(t == NULL){
		pr_err("no such pid\n");
		rcu_read_unlock();
		return -ENODEV;
	}
	rcu_read_unlock();

	/* send the signal */
	ret = send_sig_info(SIG_TEST, &info, t);    //send the signal
	if (ret < 0) {
		pr_err("error sending signal\n");
		return ret;
	}

	return count;
}

static const struct file_operations my_fops = {
	.write = write_pid,
};

static int __init ksig_init(void)
{
	pr_info("into %s\n", __func__);
	/* we need to know the pid of the user space process
 	 * -> we use debugfs for this. As soon as a pid is written to 
 	 * this file, a signal is sent to that pid
 	 */
	/* only root can write to this file (no read) */
	file = debugfs_create_file("signalconfpid", 0200, NULL, NULL, &my_fops);
	return 0;
}
static void __exit ksig_exit(void)
{
	pr_info("exit ksig module\n");
	debugfs_remove(file);
}

module_init(ksig_init);
module_exit(ksig_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ksig module");
MODULE_AUTHOR("daveti");
