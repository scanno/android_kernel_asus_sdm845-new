#define pr_fmt(fmt)	"FG: %s: " fmt, __func__

#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/of_batterydata.h>
#include <linux/platform_device.h>
#include <linux/iio/consumer.h>
#include <linux/qpnp/qpnp-revid.h>
#include <linux/proc_fs.h>
#include <linux/qpnp/qpnp-adc.h>
#include <linux/extcon.h>
#include "fg-core.h"
#include "fg-reg.h"
#include "asus_fg.h"

static struct qpnp_adc_tm_chip *g_adc_tm_dev;
static struct qpnp_adc_tm_btm_param g_adc_param;
struct extcon_dev water_detect_dev;
struct extcon_dev battery_version_dev;
struct extcon_dev battery_id_dev;
static int g_wp_state, g_liquid_state, g_wmsg_state;
static int low_thr_det, high_thr_det_1, high_thr_det_2, liquid_high_bound, liquid_low_bound;
bool g_wp_enable = false;
char g_battery_version[40] = {0};
bool g_ready_to_report_1 = false;

extern bool g_boot_complete;

struct delayed_work check_water_proof_work;

static const unsigned int vbus_liquid_ext_supported_cable[] = {
	EXTCON_NONE,
};
static const unsigned int battery_version_ext_supported_cable[] = {
	EXTCON_NONE,
};
static const unsigned int battery_id_ext_supported_cable[] = {
	EXTCON_NONE,
};

int setup_vadc_monitor(struct fg_chip *chip);

/* enum qpnp_state_request:
 * 0 - ADC_TM_HIGH_THR_ENABLE/ADC_TM_COOL_THR_ENABLE
 * 1 - ADC_TM_LOW_THR_ENABLE/ADC_TM_WARM_THR_ENABLE
 * 2 - ADC_TM_HIGH_LOW_THR_ENABLE
 * 3 - ADC_TM_HIGH_THR_DISABLE/ADC_TM_COOL_THR_DISABLE
 * 4 - ADC_TM_LOW_THR_DISABLE/ADC_TM_WARM_THR_DISABLE
 * 5 - ADC_TM_HIGH_LOW_THR_DISABLE
 */
static ssize_t vadc_enable_proc_write(struct file *filp, const char __user *buff, size_t len, loff_t *data){
	int val;
	char messages[8]="";
	int rc=0;

	len =(len > 8 ?8:len);
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);

	if(val == 1){
		g_adc_param.state_request = ADC_TM_HIGH_LOW_THR_ENABLE;// 2
		g_wp_enable = true;
	}else{
		g_adc_param.state_request = ADC_TM_HIGH_LOW_THR_DISABLE;// 5
		g_wp_enable = false;
		if (g_ready_to_report_1 || g_charger_mode)
			extcon_set_state_sync(&water_detect_dev, 0, 0);
	}

	/* Get the ADC device instance (one time) */
	g_adc_tm_dev = qpnp_get_adc_tm(g_fgChip->dev, "water-detection");
	if (IS_ERR(g_adc_tm_dev)) {
			rc = PTR_ERR(g_adc_tm_dev);
			BAT_DBG("%s qpnp_get_adc_tm fail(%d)\n", __func__, rc);
	}

	rc = qpnp_adc_tm_channel_measure(g_adc_tm_dev, &g_adc_param);
	if (rc){
		BAT_DBG_E("%s: qpnp_adc_tm_channel_measure fail(%d) ---\n", __func__,rc);
	}

	BAT_DBG("%s: enable vadc btm (%d)\n",__func__,g_adc_param.state_request);

	return len;
}


static int vadc_enable_proc_read(struct seq_file *buf, void *v)
{
	int result = g_adc_param.state_request;

	BAT_DBG("%s: %d\n", __func__, result);
	seq_printf(buf, "%d\n", result);
	return 0;
}
static int vadc_enable_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, vadc_enable_proc_read, NULL);
}

static void create_vadc_enable_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  vadc_enable_proc_open,
		.write = vadc_enable_proc_write,
		.read = seq_read,
		.llseek = seq_lseek,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/water_proof_enable", 0666, NULL, &proc_fops);
	if (!proc_file) {
		BAT_DBG_E("[Proc]%s failed!\n", __func__);
	}
	return;
}


static ssize_t vadc_high_thr_1_proc_write(struct file *filp, const char __user *buff, size_t len, loff_t *data){
	int val;
	char messages[8]="";

	len =(len > 8 ?8:len);
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);

//	g_adc_param.high_thr = val;
	high_thr_det_1 = val;

	BAT_DBG("%s: set vadc high_thr_1 (%d)\n",__func__, high_thr_det_1);

	return len;
}

static int vadc_high_thr_1_proc_read(struct seq_file *buf, void *v)
{
//	int result = g_adc_param.high_thr;
	int result = high_thr_det_1;

	BAT_DBG("%s: %d\n", __func__, result);
	seq_printf(buf, "%d\n", result);
	return 0;
}
static int vadc_high_thr_1_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, vadc_high_thr_1_proc_read, NULL);
}

static void create_vadc_high_thr_1_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  vadc_high_thr_1_proc_open,
		.write = vadc_high_thr_1_proc_write,
		.read = seq_read,
		.llseek = seq_lseek,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/vadc_high_thr_1", 0666, NULL, &proc_fops);
	if (!proc_file) {
		BAT_DBG_E("[Proc]%s failed!\n", __func__);
	}
	return;
}


static ssize_t vadc_high_thr_2_proc_write(struct file *filp, const char __user *buff, size_t len, loff_t *data){
	int val;
	char messages[8]="";

	len =(len > 8 ?8:len);
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);

//	g_adc_param.high_thr = val;
	high_thr_det_2 = val;

	BAT_DBG("%s: set vadc high_thr_2 (%d)\n",__func__, high_thr_det_2);

	return len;
}

static int vadc_high_thr_2_proc_read(struct seq_file *buf, void *v)
{
//	int result = g_adc_param.high_thr;
	int result = high_thr_det_2;

	BAT_DBG("%s: %d\n", __func__, result);
	seq_printf(buf, "%d\n", result);
	return 0;
}
static int vadc_high_thr_2_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, vadc_high_thr_2_proc_read, NULL);
}

static void create_vadc_high_thr_2_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  vadc_high_thr_2_proc_open,
		.write = vadc_high_thr_2_proc_write,
		.read = seq_read,
		.llseek = seq_lseek,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/vadc_high_thr_2", 0666, NULL, &proc_fops);
	if (!proc_file) {
		BAT_DBG_E("[Proc]%s failed!\n", __func__);
	}
	return;
}


static ssize_t vadc_low_thr_proc_write(struct file *filp, const char __user *buff, size_t len, loff_t *data){
	int val;
	char messages[8]="";
	
	len =(len > 8 ?8:len);
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);

//	g_adc_param.low_thr = val;
	low_thr_det = val;

	BAT_DBG("%s: set vadc low_thr (%d)\n",__func__, low_thr_det);

	return len;
}

static int vadc_low_thr_proc_read(struct seq_file *buf, void *v)
{
//	int result = g_adc_param.low_thr;
	int result = low_thr_det;

	BAT_DBG("%s: %d\n", __func__, result);
	seq_printf(buf, "%d\n", result);
	return 0;
}
static int vadc_low_thr_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, vadc_low_thr_proc_read, NULL);
}

static void create_vadc_low_thr_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  vadc_low_thr_proc_open,
		.write = vadc_low_thr_proc_write,
		.read = seq_read,
		.llseek = seq_lseek,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/vadc_low_thr", 0666, NULL, &proc_fops);
	if (!proc_file) {
		BAT_DBG_E("[Proc]%s failed!\n", __func__);
	}
	return;
}


static ssize_t liquid_high_bound_proc_write(struct file *filp, const char __user *buff, size_t len, loff_t *data){
	int val;
	char messages[8]="";

	len =(len > 8 ?8:len);
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);

	liquid_high_bound = val;

	BAT_DBG("%s: set liquid_high_bound (%d)\n",__func__, liquid_high_bound);

	return len;
}

static int liquid_high_bound_proc_read(struct seq_file *buf, void *v)
{
	int result = liquid_high_bound;

	BAT_DBG("%s: %d\n", __func__, result);
	seq_printf(buf, "%d\n", result);
	return 0;
}
static int liquid_high_bound_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, liquid_high_bound_proc_read, NULL);
}

static void create_liquid_high_bound_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  liquid_high_bound_proc_open,
		.write = liquid_high_bound_proc_write,
		.read = seq_read,
		.llseek = seq_lseek,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/liquid_high_bound", 0666, NULL, &proc_fops);
	if (!proc_file) {
		BAT_DBG_E("[Proc]%s failed!\n", __func__);
	}
	return;
}


static ssize_t liquid_low_bound_proc_write(struct file *filp, const char __user *buff, size_t len, loff_t *data){
	int val;
	char messages[8]="";

	len =(len > 8 ?8:len);
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);

	liquid_low_bound = val;

	BAT_DBG("%s: set liquid_low_bound (%d)\n",__func__, liquid_low_bound);

	return len;
}

static int liquid_low_bound_proc_read(struct seq_file *buf, void *v)
{
	int result = liquid_low_bound;

	BAT_DBG("%s: %d\n", __func__, result);
	seq_printf(buf, "%d\n", result);
	return 0;
}
static int liquid_low_bound_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, liquid_low_bound_proc_read, NULL);
}

static void create_liquid_low_bound_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  liquid_low_bound_proc_open,
		.write = liquid_low_bound_proc_write,
		.read = seq_read,
		.llseek = seq_lseek,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/liquid_low_bound", 0666, NULL, &proc_fops);
	if (!proc_file) {
		BAT_DBG_E("[Proc]%s failed!\n", __func__);
	}
	return;
}


//ASUS_BSP LiJen config PMIC GPIO12 to ADC channel +++
static int water_detection_adc_proc_file_proc_read(struct seq_file *buf, void *v)
{
	int result = 0, err=0;
	struct qpnp_vadc_chip *vadc_dev;
	struct qpnp_vadc_result adc_result;
	int32_t adc;
	
	vadc_dev = qpnp_get_vadc(g_fgChip->dev, "water-detection");
	if (IS_ERR(vadc_dev)) {
		BAT_DBG("%s: qpnp_get_vadc failed\n", __func__);
		result = 0;
	}else{
		err = qpnp_vadc_read(vadc_dev, VADC_AMUX5_GPIO_PU3, &adc_result); //Read the GPIO12 VADC channel with 1:1 scaling
		adc = (int) adc_result.physical;
		//adc = adc / 1000; /* uV to mV */
		BAT_DBG("%s: adc=%d\n", __func__, adc);
		result = adc;
	}

	BAT_DBG("%s: %d\n", __func__, result);
	seq_printf(buf, "%d\n", result);
	return 0;
}
static int water_detection_adc_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, water_detection_adc_proc_file_proc_read, NULL);
}

static void create_water_detection_adc_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  water_detection_adc_proc_open,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/water_detection_adc", 0444, NULL, &proc_fops);
	if (!proc_file) {
		BAT_DBG_E("[Proc]%s failed!\n", __func__);
	}
	return;
}

static int wp_state_proc_file_proc_read(struct seq_file *buf, void *v)
{
	BAT_DBG("%s: %d\n", __func__, g_wp_state);
	seq_printf(buf, "%d\n", g_wp_state);
	return 0;
}
static int wp_state_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, wp_state_proc_file_proc_read, NULL);
}

static void create_wp_state_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  wp_state_proc_open,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/wp_state", 0444, NULL, &proc_fops);
	if (!proc_file) {
		BAT_DBG_E("[Proc]%s failed!\n", __func__);
	}
	return;
}

static int liquid_state_proc_file_proc_read(struct seq_file *buf, void *v)
{
	BAT_DBG("%s: %d\n", __func__, g_liquid_state);
	seq_printf(buf, "%d\n", g_liquid_state);
	return 0;
}
static int liquid_state_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, liquid_state_proc_file_proc_read, NULL);
}

static void create_liquid_state_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  liquid_state_proc_open,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/liquid_state", 0444, NULL, &proc_fops);
	if (!proc_file) {
		BAT_DBG_E("[Proc]%s failed!\n", __func__);
	}
	return;
}

static int wmsg_state_proc_file_proc_read(struct seq_file *buf, void *v)
{
	BAT_DBG("%s: %d\n", __func__, g_wmsg_state);
	seq_printf(buf, "%d\n", g_wmsg_state);
	return 0;
}
static int wmsg_state_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, wmsg_state_proc_file_proc_read, NULL);
}

static void create_wmsg_state_proc_file(void)
{
	static const struct file_operations proc_fops = {
		.owner = THIS_MODULE,
		.open =  wmsg_state_proc_open,
		.read = seq_read,
		.release = single_release,
	};
	struct proc_dir_entry *proc_file = proc_create("driver/wmsg_state", 0444, NULL, &proc_fops);
	if (!proc_file) {
		BAT_DBG_E("[Proc]%s failed!\n", __func__);
	}
	return;
}
//ASUS_BSP LiJen config PMIC GPIO12 to ADC channel ---


//ASUS_BSP LiJen config PMIC GPIO12 VADC_BTM channel +++
static int32_t get_vadc_voltage(void){
	struct qpnp_vadc_chip *vadc_dev;
	struct qpnp_vadc_result adc_result;
	int32_t adc;

	vadc_dev = qpnp_get_vadc(g_fgChip->dev, "water-detection");
	if (IS_ERR(vadc_dev)) {
		BAT_DBG("%s: qpnp_get_vadc failed\n", __func__);
		return -1;
	}else{
		qpnp_vadc_read(vadc_dev, VADC_AMUX5_GPIO_PU3, &adc_result); //Read the GPIO12 VADC channel with 1:1 scaling
		adc = (int) adc_result.physical;
		//adc = adc / 1000; /* uV to mV */
		BAT_DBG("%s: adc=%d\n", __func__, adc);
	}

	return adc;
}

static void vadc_notification(enum qpnp_tm_state state, void *ctx)
{
	if (state == ADC_TM_HIGH_STATE) { //In High state
		pr_err("%s: ADC_TM_HIGH_STATE\n", __func__);
	}
	else { //In low state
		pr_err("%s: ADC_TM_LOW_STATE\n", __func__);
	}

	setup_vadc_monitor(ctx);
}

bool double_check_is_liquid_mode(int32_t adc)
{
	int32_t adc1=-1, adc2=-1, adc3=-1, adc4=-1, adc5=-1;
	bool ret;

	adc1 = adc;
	msleep(300);
	adc2 = get_vadc_voltage();
	if(adc2 >= 200000 && adc2 <= 1700000){
		msleep(300);
		adc3 = get_vadc_voltage();
		if(adc3 >= 200000 && adc3 <= 1700000){
			msleep(300);
			adc4 = get_vadc_voltage();
			if(adc4 >= 200000 && adc4 <= 1700000){
				msleep(300);
				adc5 = get_vadc_voltage();
				if(adc5 >= 200000 && adc5 <= 1700000){
					ret = true;
					BAT_DBG("%s: adc(%d, %d, %d, %d, %d) ret(%d)\n", __func__, adc1, adc2,adc3, adc4, adc5, ret);
					return ret;
				}
			}
		}
	}

	ret = false;
	BAT_DBG("%s: adc(%d, %d, %d, %d, %d) ret(%d)\n", __func__, adc1, adc2,adc3, adc4, adc5, ret);
	return ret;
}

int setup_vadc_monitor(struct fg_chip *chip)
{
	int rc=0;
	int32_t adc;

restart:
	adc = get_vadc_voltage();

	/* Get the ADC device instance (one time) */
	g_adc_tm_dev = qpnp_get_adc_tm(chip->dev, "water-detection");
	if (IS_ERR(g_adc_tm_dev)) {
			rc = PTR_ERR(g_adc_tm_dev);
			BAT_DBG("%s qpnp_get_adc_tm fail(%d)\n", __func__, rc);
	}

	if(adc > liquid_high_bound){ //Normal
		g_adc_param.low_thr = low_thr_det; //uV
		g_adc_param.state_request = ADC_TM_LOW_THR_ENABLE;
		g_wp_state = 0;
		g_liquid_state = 0;
		g_wmsg_state = 0;
	}else if(adc < liquid_low_bound){ //Cable in
		g_adc_param.high_thr = high_thr_det_2; //uV
		g_adc_param.state_request = ADC_TM_HIGH_THR_ENABLE;
		g_wp_state = 2;
		g_liquid_state = 0;
		g_wmsg_state = 0;
	}else{	//With Liquid
		if (g_wp_state != 1) {
			if (double_check_is_liquid_mode(adc)) {
				g_adc_param.high_thr = high_thr_det_1; //uV
				g_adc_param.state_request = ADC_TM_HIGH_THR_ENABLE; 
				g_wp_state = 1;
				g_liquid_state = 1;
				g_wmsg_state = 1;
                ASUSErclog(ASUS_WATER_ALERT, "Water_alert is triggered");
			} else {
				goto restart;
			}
		} else {}
	}

    if (g_wp_enable) {
        // set state
        if (g_ready_to_report_1 || g_charger_mode)
			extcon_set_state_sync(&water_detect_dev, 0, g_wmsg_state);
    }

//	pr_err("%s low_thr(%d), high_thr(%d), state_request(%d)\n", __func__, g_adc_param.low_thr, g_adc_param.high_thr, g_adc_param.state_request);
	pr_err("%s adc(%d), g_wp_state(%d), low_thr(%d), high_thr(%d), state_request(%d)\n", __func__, adc, g_wp_state, g_adc_param.low_thr, g_adc_param.high_thr, g_adc_param.state_request);
    ASUSEvtlog("%s adc(%d), g_wp_state(%d), low_thr(%d), high_thr(%d), state_request(%d)\n", __func__, adc, g_wp_state, g_adc_param.low_thr, g_adc_param.high_thr, g_adc_param.state_request);
	g_adc_param.channel = 0x76;
	g_adc_param.timer_interval = ADC_MEAS2_INTERVAL_1S;
	g_adc_param.btm_ctx = chip;
	g_adc_param.threshold_notification = vadc_notification;
	rc = qpnp_adc_tm_channel_measure(g_adc_tm_dev, &g_adc_param);
	if (rc){
		BAT_DBG_E("%s: qpnp_adc_tm_channel_measure fail(%d) ---\n", __func__,rc);
	}

	return rc;
}
//ASUS_BSP LiJen config PMIC GPIO12 VADC_BTM channel ---

void asus_check_water_proof_work(struct work_struct *work)
{
	static bool first_time = true;
    if (g_boot_complete) {
		if (first_time) {
			printk("[BAT][CHG] water_proof: boot completed first time, g_wmsg_state status = %d, report UI 5s later\n", g_wmsg_state);
			first_time = false;
			schedule_delayed_work(&check_water_proof_work, msecs_to_jiffies(5000));
			return;
		}
		g_ready_to_report_1 = true;
        printk("[BAT][CHG] water_proof: boot completed, g_wmsg_state status = %d\n", g_wmsg_state);
		// report the event to UI when ims ready
		extcon_set_state_sync(&water_detect_dev, 0, g_wmsg_state);
    } else {
        schedule_delayed_work(&check_water_proof_work, msecs_to_jiffies(5000));
        printk("[BAT][CHG] water_proof: boot NOT completed yet, retry 5s\n");
    }
}

void asus_water_detection_init(struct fg_chip *chip)
{
	int rc=0;
	g_adc_param.low_thr = 1600000;//uv
	g_wp_state = 0;
	g_liquid_state = 0;
	g_wmsg_state = 0;
	low_thr_det = 1600000;//uv
	high_thr_det_1 = 1700000;//uv
	high_thr_det_2 = 300000;//uv
	liquid_high_bound = 1700000;//uv
	liquid_low_bound = 200000;//uv

	water_detect_dev.supported_cable = vbus_liquid_ext_supported_cable;
	water_detect_dev.name = "vbus_liquid";
	dev_set_name(&water_detect_dev.dev, "vbus_liquid");
	rc = extcon_dev_register(&water_detect_dev);
	if (rc) {
		pr_err("vbus liquid registration failed");
		return;
	}

	rc = setup_vadc_monitor(chip);
	if(rc){
		BAT_DBG_E("%s: setup_vadc_monitor fail(%d) ---\n", __func__,rc);
	} else {
        if (g_charger_mode == 0 && g_wp_enable) {
            INIT_DELAYED_WORK(&check_water_proof_work, asus_check_water_proof_work);
            schedule_delayed_work(&check_water_proof_work, 0);
        }
	}
}

void asus_battery_version_init(void)
{
    int rc = 0;

    battery_version_dev.supported_cable = battery_version_ext_supported_cable;
    battery_version_dev.name = "C11Pxxxx";
    dev_set_name(&battery_version_dev.dev, "battery");
    rc = extcon_dev_register(&battery_version_dev);
    if (rc) {
        pr_err("battery version registration failed");
    }
}

void asus_set_battery_version(void)
{
    char Battery_Model_Name[10] = "C11Pxxxx";
    char Cell_Supplier_Code = 'X';
    char battery_id[3] = "00";
    char profile_version[5] = "0001";
    char sw_driver_version[15] = "80.03.78.37";

    if(g_fgChip->batt_id_ohms <= 56100 && g_fgChip->batt_id_ohms >= 45900) {
        Cell_Supplier_Code = 'O';
        strncpy(battery_id, "01", sizeof(battery_id));
    }
    else {
        pr_info("Unknow battery \n");
    }
    strncpy(Battery_Model_Name, "C11P1708", sizeof(Battery_Model_Name));
    strncpy( profile_version, "0002", sizeof(profile_version));

    memset(g_battery_version, 0, sizeof(g_battery_version));
    snprintf(g_battery_version, sizeof(g_battery_version), "%s-%c-%s-%s-%s", Battery_Model_Name, Cell_Supplier_Code, battery_id, profile_version, sw_driver_version);
    printk("battery_version = %s\n", g_battery_version);
    battery_version_dev.name = (const char *)g_battery_version;
}

void asus_battery_id_init(void)
{
    int rc = 0;

    battery_id_dev.supported_cable = battery_id_ext_supported_cable;
    battery_id_dev.name = "battery_id";
    dev_set_name(&battery_id_dev.dev, "battery_id");
    rc = extcon_dev_register(&battery_id_dev);
    if (rc) {
        pr_err("battery id registration failed");
    }
}

void asus_check_batt_id(struct fg_chip *chip)
{
    bool in_range = (chip->batt_id_ohms <= 56100 && chip->batt_id_ohms >= 45900);

    pr_info("%s: batt_id_ohms = %d, in_range = %d\n", __func__, chip->batt_id_ohms, in_range);
    extcon_set_state_sync(&battery_id_dev, 0, in_range);
}

#ifdef ASUS_FACTORY_BUILD
#define gaugeIC_status_PROC_FILE "driver/gaugeIC_status"

static int gaugeIC_status_proc_read(struct seq_file *buf, void *v)
{
	u8 value;
	int rc = -1;

	if(!g_fgChip || !g_fgChip->mem_if_base){
		pr_err("g_fgChip is NULL or addr is ZERO !\n");
		seq_printf(buf, "0\n");
		return rc;
	}

    rc = fg_read(g_fgChip, MEM_IF_INT_RT_STS(g_fgChip), &value, 1);
	if (rc) {
		seq_printf(buf, "0\n");
		pr_err("proc read INT_RT_STS vaule failed!\n");
		return rc;
	}

	seq_printf(buf, "1\n");
	return 0;
}

static int gaugeIC_status_proc_open(struct inode *inode, struct  file *file)
{
	return single_open(file, gaugeIC_status_proc_read, NULL);
}

static const struct file_operations gaugeIC_status_fops = {
	.owner = THIS_MODULE,
	.open = gaugeIC_status_proc_open,
	.read = seq_read,
};

static void create_asus_ATD_gaugeIC_status_interface(void)
{
	struct proc_dir_entry *gaugeIC_status_proc_file = proc_create(gaugeIC_status_PROC_FILE, 0444, NULL, &gaugeIC_status_fops);

	if (gaugeIC_status_proc_file) {
		printk("[FG][Proc]gaugeIC_status create ok!\n");
	} else{
		printk("[FG][Proc]gaugeIC_status create failed!\n");
	}
}

#define ATD_battery_current_PROC_FILE "driver/battery_current"

static int ATD_battery_current_proc_read(struct seq_file *buf, void *data)
{
    int rc;
    union power_supply_propval current_now;

    if(!g_fgChip){
        pr_err("fgchip is NULL!\n");
        return -1;
    }

    rc = power_supply_get_property(g_fgChip->fg_psy, POWER_SUPPLY_PROP_CURRENT_NOW, &current_now);
    if (rc) {
        pr_err("cannot read batt current\n");
        return -1;
    }

    seq_printf(buf, "%d\n", (current_now.intval /1000 * -1));

    return 0;
}
static int ATD_battery_current_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, ATD_battery_current_proc_read, NULL);
}

static const struct file_operations ATD_battery_current_fops = {
	.owner = THIS_MODULE,
	.open = ATD_battery_current_proc_open,
	.read = seq_read,
};
void create_asus_ATD_battery_current_interface(void)
{
	struct proc_dir_entry *ATD_battery_current = proc_create(ATD_battery_current_PROC_FILE, 0444, NULL, &ATD_battery_current_fops);
	if(!ATD_battery_current)
		pr_err("creat ATD_battery_current proc inode failed!\n");
}

#define ATD_battery_voltage_PROC_FILE "driver/battery_voltage"
static int ATD_battery_voltage_proc_read(struct seq_file *buf, void *data)
{
    int rc;
    union power_supply_propval voltage_now;

    if(!g_fgChip){
        pr_err("fgchip is NULL!\n");
        return -1;
    }

    rc = power_supply_get_property(g_fgChip->fg_psy, POWER_SUPPLY_PROP_VOLTAGE_NOW, &voltage_now);
    if (rc) {
        pr_err("cannot read batt voltage\n");
        return -1;
    }

    seq_printf(buf, "%d\n", (voltage_now.intval /1000));

	return 0;
}

static int ATD_battery_voltage_proc_open(struct inode *inode, struct  file *file)
{
	return single_open(file, ATD_battery_voltage_proc_read, NULL);
}

static const struct file_operations ATD_battery_voltage_fops = {
	.owner = THIS_MODULE,
	.open = ATD_battery_voltage_proc_open,
	.read = seq_read,
};

void create_asus_ATD_battery_voltage_interface(void)
{
	struct proc_dir_entry *ATD_battery_voltage = proc_create(ATD_battery_voltage_PROC_FILE, 0444, NULL, &ATD_battery_voltage_fops);
	if(!ATD_battery_voltage)
		pr_err("creat ATD_battery_voltage proc inode failed!\n");
}

#endif

static ssize_t thermal_test_proc_write(struct file *filp, const char __user *buff, size_t len, loff_t *data){
	int val;
	char messages[8]="";

	len =(len > 8 ?8:len);
	if (copy_from_user(messages, buff, len)) {
		return -EFAULT;
	}
	val = (int)simple_strtol(messages, NULL, 10);

	if(val < -200 ||val > 700)
		fake_temp = FAKE_TEMP_INIT;
	else
		fake_temp = val;

	BAT_DBG("%s: set fake temperature as %d\n",__func__,fake_temp);

	return len;
}

static int thermal_proc_read(struct seq_file *buf, void *v)
{
	int result = fake_temp;

	BAT_DBG("%s: %d\n", __func__, result);
	seq_printf(buf, "%d\n", result);
	return 0;
}
static int thermal_test_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, thermal_proc_read, NULL);
}

static const struct file_operations thermal_test_temp_fops = {
	.owner = THIS_MODULE,
	.open =  thermal_test_proc_open,
	.write = thermal_test_proc_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static void create_thermal_test_proc_file(void)
{

	struct proc_dir_entry *proc_file = proc_create("driver/ThermalTemp", 0666, NULL, &thermal_test_temp_fops);
	if (!proc_file) {
		BAT_DBG_E("[Proc]%s failed!\n", __func__);
	}
	return;
}

void asus_procfs_create(void)
{
	create_water_detection_adc_proc_file();
	create_wp_state_proc_file();
	create_liquid_state_proc_file();
	create_wmsg_state_proc_file();
	create_vadc_enable_proc_file();
	create_vadc_high_thr_1_proc_file();
	create_vadc_high_thr_2_proc_file();
	create_vadc_low_thr_proc_file();
	create_liquid_high_bound_proc_file();
	create_liquid_low_bound_proc_file();
	create_thermal_test_proc_file();
#ifdef ASUS_FACTORY_BUILD
	create_asus_ATD_gaugeIC_status_interface();
	create_asus_ATD_battery_current_interface();
	create_asus_ATD_battery_voltage_interface();
#endif
}

void asus_fg_init_config(struct fg_chip *chip)
{
    int rc;
    u8	batt_aux_therm_coeffs[3] = {0xB6, 0x2D, 0xE5};

    // Set FG_ADC_RR_AUX_THERM_Cx_COEFF(0x4588/0x4589/0x458A)
    rc = fg_write(chip, chip->rradc_base + 0x88,
        batt_aux_therm_coeffs, 3);
    if (rc < 0) {
        pr_err("Error in writing battery aux thermal coefficients, rc=%d\n",
            rc);
    }
}

int asus_fg_porting(struct fg_chip *chip)
{
	if(!chip){
		BAT_DBG_E("struct fg_chip is NULL,Now will return!\n");
		return -1;
	}

	asus_fg_init_config(chip);

	asus_water_detection_init(chip);
	asus_battery_version_init();
	asus_battery_id_init();

	asus_procfs_create();

	return 0;
}
