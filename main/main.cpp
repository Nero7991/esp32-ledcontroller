
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "esp_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "freertos/queue.h"
#include "driver/periph_ctrl.h"
#include "driver/timer.h"
#include "Timer.h"
#include "Switch.h"

/**
 * Brief:
 * This test code shows how to configure gpio and how to use gpio interrupt.
 *
 * GPIO status:
 * GPIO18: output
 * GPIO19: output
 * GPIO4:  input, pulled up, interrupt from rising edge and falling edge
 * GPIO5:  input, pulled up, interrupt from rising edge.
 *
 * Test:
 * Connect GPIO18 with GPIO4
 * Connect GPIO19 with GPIO5
 * Generate pulses on GPIO18/19, that triggers interrupt on GPIO4/5
 *
 */

#define GPIO_OUTPUT_IO_0 (gpio_num_t)18
#define GPIO_OUTPUT_IO_1 (gpio_num_t)19
#define GPIO_OUTPUT_PIN_SEL ((1ULL << GPIO_OUTPUT_IO_0) | (1ULL << GPIO_OUTPUT_IO_1))
#define GPIO_INPUT_IO_0 (gpio_num_t)4
#define GPIO_INPUT_IO_1 (gpio_num_t)5
#define GPIO_INPUT_PIN_SEL ((1ULL << GPIO_INPUT_IO_0) | (1ULL << GPIO_INPUT_IO_1))
#define ESP_INTR_FLAG_DEFAULT 0

#define LEDC_HS_TIMER LEDC_TIMER_0
#define LEDC_HS_MODE LEDC_HIGH_SPEED_MODE
#define LEDC_HS_CH0_GPIO (18)
#define LEDC_HS_CH0_CHANNEL LEDC_CHANNEL_0
#define LEDC_HS_CH1_GPIO (19)
#define LEDC_HS_CH1_CHANNEL LEDC_CHANNEL_1

#define LEDC_TEST_CH_NUM (4)
#define LEDC_TEST_DUTY 100

#define INVERSE_TIME 1000
#define APU_CLK 80000000
#define TIMER_DIVIDER 8                              //  Hardware timer clock divider
#define TIMER_SCALE (TIMER_BASE_CLK / TIMER_DIVIDER) // convert counter value to seconds
#define TIMER_ALARM_VALUE APU_CLK / (TIMER_DIVIDER * INVERSE_TIME)

static xQueueHandle gpio_evt_queue = NULL;
TimerClass T1;
SwitchClass S1, S2;
extern TimerClass Timer;
volatile int SetDuty = 0, Duty = 0;

const uint16_t FadeLut[1001] = {0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 6, 6, 7, 7, 8, 9, 9, 10, 10, 11, 12, 12, 13, 14, 15, 15, 16, 17, 18, 19, 20, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 31, 32, 33, 34, 35, 36, 38, 39, 40, 41, 43, 44, 45, 47, 48, 50, 51, 52, 54, 55, 57, 58, 60, 62, 63, 65, 66, 68, 70, 71, 73, 75, 76, 78, 80, 82, 84, 86, 87, 89, 91, 93, 95, 97, 99, 101, 103, 105, 107, 109, 111, 113, 116, 118, 120, 122, 124, 127, 129, 131, 134, 136, 138, 141, 143, 145, 148, 150, 153, 155, 158, 160, 163, 165, 168, 170, 173, 176, 178, 181, 184, 187, 189, 192, 195, 198, 200, 203, 206, 209, 212, 215, 218, 221, 224, 227, 230, 233, 236, 239, 242, 245, 248, 251, 255, 258, 261, 264, 267, 271, 274, 277, 281, 284, 287, 291, 294, 298, 301, 305, 308, 312, 315, 319, 322, 326, 329, 333, 337, 340, 344, 348, 351, 355, 359, 363, 367, 370, 374, 378, 382, 386, 390, 394, 398, 402, 406, 410, 414, 418, 422, 426, 430, 434, 438, 442, 447, 451, 455, 459, 463, 468, 472, 476, 481, 485, 489, 494, 498, 503, 507, 512, 516, 521, 525, 530, 534, 539, 543, 548, 553, 557, 562, 567, 571, 576, 581, 586, 591, 595, 600, 605, 610, 615, 620, 625, 630, 634, 639, 644, 649, 654, 660, 665, 670, 675, 680, 685, 690, 695, 701, 706, 711, 716, 722, 727, 732, 738, 743, 748, 754, 759, 765, 770, 775, 781, 786, 792, 797, 803, 809, 814, 820, 825, 831, 837, 842, 848, 854, 860, 865, 871, 877, 883, 888, 894, 900, 906, 912, 918, 924, 930, 936, 942, 948, 954, 960, 966, 972, 978, 984, 990, 996, 1003, 1009, 1015, 1021, 1027, 1034, 1040, 1046, 1052, 1059, 1065, 1072, 1078, 1084, 1091, 1097, 1104, 1110, 1117, 1123, 1130, 1136, 1143, 1149, 1156, 1162, 1169, 1176, 1182, 1189, 1196, 1202, 1209, 1216, 1223, 1229, 1236, 1243, 1250, 1257, 1264, 1270, 1277, 1284, 1291, 1298, 1305, 1312, 1319, 1326, 1333, 1340, 1347, 1354, 1361, 1369, 1376, 1383, 1390, 1397, 1404, 1412, 1419, 1426, 1433, 1441, 1448, 1455, 1463, 1470, 1477, 1485, 1492, 1500, 1507, 1514, 1522, 1529, 1537, 1544, 1552, 1559, 1567, 1575, 1582, 1590, 1597, 1605, 1613, 1620, 1628, 1636, 1644, 1651, 1659, 1667, 1675, 1682, 1690, 1698, 1706, 1714, 1722, 1730, 1738, 1746, 1754, 1761, 1769, 1777, 1785, 1794, 1802, 1810, 1818, 1826, 1834, 1842, 1850, 1858, 1867, 1875, 1883, 1891, 1899, 1908, 1916, 1924, 1933, 1941, 1949, 1958, 1966, 1974, 1983, 1991, 1999, 2008, 2016, 2025, 2033, 2042, 2050, 2059, 2067, 2076, 2085, 2093, 2102, 2110, 2119, 2128, 2136, 2145, 2154, 2162, 2171, 2180, 2189, 2197, 2206, 2215, 2224, 2233, 2241, 2250, 2259, 2268, 2277, 2286, 2295, 2304, 2313, 2322, 2331, 2340, 2349, 2358, 2367, 2376, 2385, 2394, 2403, 2412, 2421, 2431, 2440, 2449, 2458, 2467, 2477, 2486, 2495, 2504, 2514, 2523, 2532, 2541, 2551, 2560, 2570, 2579, 2588, 2598, 2607, 2617, 2626, 2635, 2645, 2654, 2664, 2673, 2683, 2692, 2702, 2712, 2721, 2731, 2740, 2750, 2760, 2769, 2779, 2789, 2798, 2808, 2818, 2827, 2837, 2847, 2857, 2866, 2876, 2886, 2896, 2906, 2916, 2925, 2935, 2945, 2955, 2965, 2975, 2985, 2995, 3005, 3015, 3025, 3035, 3045, 3055, 3065, 3075, 3085, 3095, 3105, 3115, 3125, 3135, 3146, 3156, 3166, 3176, 3186, 3196, 3207, 3217, 3227, 3237, 3248, 3258, 3268, 3279, 3289, 3299, 3310, 3320, 3330, 3341, 3351, 3361, 3372, 3382, 3393, 3403, 3414, 3424, 3435, 3445, 3456, 3466, 3477, 3487, 3498, 3508, 3519, 3529, 3540, 3551, 3561, 3572, 3582, 3593, 3604, 3614, 3625, 3636, 3647, 3657, 3668, 3679, 3690, 3700, 3711, 3722, 3733, 3743, 3754, 3765, 3776, 3787, 3798, 3809, 3819, 3830, 3841, 3852, 3863, 3874, 3885, 3896, 3907, 3918, 3929, 3940, 3951, 3962, 3973, 3984, 3995, 4006, 4017, 4028, 4039, 4051, 4062, 4073, 4084, 4095, 4106, 4117, 4129, 4140, 4151, 4162, 4173, 4185, 4196, 4207, 4218, 4230, 4241, 4252, 4264, 4275, 4286, 4297, 4309, 4320, 4332, 4343, 4354, 4366, 4377, 4388, 4400, 4411, 4423, 4434, 4446, 4457, 4469, 4480, 4491, 4503, 4515, 4526, 4538, 4549, 4561, 4572, 4584, 4595, 4607, 4618, 4630, 4642, 4653, 4665, 4677, 4688, 4700, 4711, 4723, 4735, 4747, 4758, 4770, 4782, 4793, 4805, 4817, 4829, 4840, 4852, 4864, 4876, 4887, 4899, 4911, 4923, 4935, 4946, 4958, 4970, 4982, 4994, 5006, 5018, 5029, 5041, 5053, 5065, 5077, 5089, 5101, 5113, 5125, 5137, 5149, 5161, 5173, 5185, 5197, 5209, 5221, 5233, 5245, 5257, 5269, 5281, 5293, 5305, 5317, 5329, 5341, 5353, 5365, 5377, 5389, 5401, 5414, 5426, 5438, 5450, 5462, 5474, 5486, 5499, 5511, 5523, 5535, 5547, 5559, 5572, 5584, 5596, 5608, 5621, 5633, 5645, 5657, 5670, 5682, 5694, 5706, 5719, 5731, 5743, 5755, 5768, 5780, 5792, 5805, 5817, 5829, 5842, 5854, 5866, 5879, 5891, 5904, 5916, 5928, 5941, 5953, 5965, 5978, 5990, 6003, 6015, 6027, 6040, 6052, 6065, 6077, 6090, 6102, 6115, 6127, 6140, 6152, 6164, 6177, 6189, 6202, 6214, 6227, 6239, 6252, 6265, 6277, 6290, 6302, 6315, 6327, 6340, 6352, 6365, 6377, 6390, 6403, 6415, 6428, 6440, 6453, 6465, 6478, 6491, 6503, 6516, 6529, 6541, 6554, 6566, 6579, 6592, 6604, 6617, 6630, 6642, 6655, 6668, 6680, 6693, 6706, 6718, 6731, 6744, 6756, 6769, 6782, 6794, 6807, 6820, 6832, 6845, 6858, 6871, 6883, 6896, 6909, 6921, 6934, 6947, 6960, 6972, 6985, 6998, 7011, 7023, 7036, 7049, 7062, 7074, 7087, 7100, 7113, 7126, 7138, 7151, 7164, 7177, 7189, 7202, 7215, 7228, 7241, 7253, 7266, 7279, 7292, 7305, 7317, 7330, 7343, 7356, 7369, 7382, 7394, 7407, 7420, 7433, 7446, 7459, 7471, 7484, 7497, 7510, 7523, 7536, 7548, 7561, 7574, 7587, 7600, 7613, 7626, 7638, 7651, 7664, 7677, 7690, 7703, 7716, 7728, 7741, 7754, 7767, 7780, 7793, 7806, 7818, 7831, 7844, 7857, 7870, 7883, 7896, 7909, 7922, 7934, 7947, 7960, 7973, 7986, 7999, 8012, 8025, 8037, 8050, 8063, 8076, 8089, 8102, 8115, 8128, 8141, 8153, 8166, 8179, 8191, 8191, 8191, 8191, 8191, 8191};

/*
* Prepare and set configuration of timers
* that will be used by LED Controller
*/
ledc_channel_config_t ledc_channel[LEDC_TEST_CH_NUM] = {
    {
        LEDC_HS_CH0_GPIO,
        LEDC_HS_MODE,
        LEDC_HS_CH0_CHANNEL,
        LEDC_INTR_DISABLE,
        LEDC_HS_TIMER,
        0,
        0,
    },
    {
        LEDC_HS_CH1_GPIO,
        LEDC_HS_MODE,
        LEDC_HS_CH1_CHANNEL,
        LEDC_INTR_DISABLE,
        LEDC_HS_TIMER,
        0,
        0,
    }};

static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)arg;
    S1.pinStateChanged();
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

volatile uint64_t cnt = 0;
static void IRAM_ATTR timer_group0_isr(void *para)
{
    timer_spinlock_take(TIMER_GROUP_0);
    int timer_idx = (int)para;
    cnt += 1;
    /* Retrieve the interrupt status and the counter value
       from the timer that reported the interrupt */
    uint32_t timer_intr = timer_group_get_intr_status_in_isr(TIMER_GROUP_0);

    /* Clear the interrupt
       and update the alarm time for the timer with with reload */
    if (timer_intr & TIMER_INTR_T0)
    {
        timer_group_clr_intr_status_in_isr(TIMER_GROUP_0, TIMER_0);
        timer_group_set_alarm_value_in_isr(TIMER_GROUP_0, (timer_idx_t)timer_idx, TIMER_ALARM_VALUE);
    }
    /* After the alarm has been triggered
      we need enable it again, so it is triggered the next time */
    timer_group_enable_alarm_in_isr(TIMER_GROUP_0, (timer_idx_t)timer_idx);
    Timer.milliHappened((uint8_t)timer_idx);
    timer_spinlock_give(TIMER_GROUP_0);
}

static void gpio_task_example(void *arg)
{
    uint32_t io_num;
    for (;;)
    {
        printf("Running");
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY))
        {
            printf("GPIO[%d] intr, val: %d\n", io_num, gpio_get_level((gpio_num_t)io_num));
        }
    }
}

volatile int FadeLutPointer = 0, SetLutPointer = 1000, CurrentLutPointer = 0;
volatile bool FadeDirection = 0;
volatile bool FadeEnabled = 0;

void timerEnded(uint8_t TimerId)
{
    if (FadeEnabled)
    {
        if (!FadeDirection)
        {
            Duty = FadeLut[FadeLutPointer];
            FadeLutPointer += 1;
            if (FadeLutPointer == 1000)
                FadeDirection = 1;
        }
        else
        {
            Duty = FadeLut[FadeLutPointer];
            FadeLutPointer -= 1;
            if (FadeLutPointer == 0)
                FadeDirection = 0;
        }
    }
    else
    {
        if (CurrentLutPointer != SetLutPointer)
        {
           
            if (CurrentLutPointer < SetLutPointer)
            {
                CurrentLutPointer += 1;
            }
            else
            {
                CurrentLutPointer -= 1;
            }
            Duty = FadeLut[CurrentLutPointer];
        }
    }
    uint16_t ch;
    for (ch = 0; ch < LEDC_TEST_CH_NUM; ch++)
    {
        ledc_set_duty(ledc_channel[ch].speed_mode, ledc_channel[ch].channel, Duty);
        ledc_update_duty(ledc_channel[ch].speed_mode, ledc_channel[ch].channel);
    }
}

void toggleLight(uint8_t SwitchId){
    if(SetLutPointer > 500)
    SetLutPointer = 0;
    else
    {
        SetLutPointer = 1000;
    }
}

void toggleFadeMode(uint8_t SwitchId){
    FadeEnabled ^= 1;
}

void upButtonPressed(uint8_t SwitchId)
{
    if(SetLutPointer < 1000)
    SetLutPointer += 100;
    if(SetLutPointer > 1000)
    SetLutPointer = 1000;
}

void downButtonPressed(uint8_t SwitchId)
{
    if(SetLutPointer > 0)
    SetLutPointer -= 100;
}

void setupLedPwm();
void setupGpio();
void setupGroup0Timer0();

extern "C" void app_main(void)
{
    setupLedPwm();
    setupGpio();
    setupGroup0Timer0();

    T1.initializeTimer();
    T1.setCallBackTime(1, true, timerEnded);

    S1.initializeSwitch(4, &S1);
    S2.initializeSwitch(5, &S2);

    S1.shortPress(upButtonPressed);
    S2.shortPress(downButtonPressed);
    S1.longPress(toggleLight);
    S2.longPress(toggleFadeMode);
    while (1)
    {

        printf("Time: %" PRIu64 "\n", Timer.millis());
        vTaskDelay(1000 / portTICK_RATE_MS);
        gpio_set_level(GPIO_OUTPUT_IO_0, cnt % 2);
        gpio_set_level(GPIO_OUTPUT_IO_1, cnt % 2);
        printf("3. LEDC set duty = %d\n", Duty);
        // for (ch = 0; ch < LEDC_TEST_CH_NUM; ch++) {
        //     ledc_set_duty(ledc_channel[ch].speed_mode, ledc_channel[ch].channel, Duty);
        //     ledc_update_duty(ledc_channel[ch].speed_mode, ledc_channel[ch].channel);
        // }
    }
}

void setupLedPwm()
{
    uint16_t ch;
    ledc_timer_config_t ledc_timer = {
        LEDC_HS_MODE,      // resolution of PWM duty
        LEDC_TIMER_13_BIT, // frequency of PWM signal
        LEDC_HS_TIMER,     // timer mode
        5000,              // timer index
        LEDC_AUTO_CLK,     // Auto select the source clock
    };
    // Set configuration of timer0 for high speed channels
    ledc_timer_config(&ledc_timer);

    for (ch = 0; ch < LEDC_TEST_CH_NUM; ch++)
    {
        ledc_channel_config(&ledc_channel[ch]);
    }
}

void setupGpio()
{
    gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = (gpio_int_type_t)GPIO_PIN_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    //disable pull-down mode
    io_conf.pull_down_en = (gpio_pulldown_t)0;
    //disable pull-up mode
    io_conf.pull_up_en = (gpio_pullup_t)0;
    //configure GPIO with the given settings
    //gpio_config(&io_conf);

    //interrupt of rising edge
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    //bit mask of the pins, use GPIO4/5 here
    io_conf.pin_bit_mask = GPIO_INPUT_PIN_SEL;
    //set as input mode
    io_conf.mode = GPIO_MODE_INPUT;
    //enable pull-up mode
    io_conf.pull_up_en = (gpio_pullup_t)1;
    gpio_config(&io_conf);

    //change gpio intrrupt type for one pin
    //gpio_set_intr_type(GPIO_INPUT_IO_0, GPIO_INTR_ANYEDGE);

    //create a queue to handle gpio event from isr
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    //start gpio task
    xTaskCreate(gpio_task_example, "gpio_task_example", 2048, NULL, 10, NULL);

    //install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_INPUT_IO_0, gpio_isr_handler, (void *)GPIO_INPUT_IO_0);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(GPIO_INPUT_IO_1, gpio_isr_handler, (void *)GPIO_INPUT_IO_1);

    //remove isr handler for gpio number.
    gpio_isr_handler_remove(GPIO_INPUT_IO_0);
    //hook isr handler for specific gpio pin again
    gpio_isr_handler_add(GPIO_INPUT_IO_0, gpio_isr_handler, (void *)GPIO_INPUT_IO_0);
}

void setupGroup0Timer0()
{
    timer_config_t config;
    config.divider = TIMER_DIVIDER;
    config.counter_dir = TIMER_COUNT_UP;
    config.counter_en = TIMER_PAUSE;
    config.alarm_en = TIMER_ALARM_EN;
    config.intr_type = TIMER_INTR_LEVEL;
    config.auto_reload = (timer_autoreload_t) true;
#ifdef TIMER_GROUP_SUPPORTS_XTAL_CLOCK
    config.clk_src = TIMER_SRC_CLK_APB;
#endif
    timer_init(TIMER_GROUP_0, TIMER_0, &config);

    /* Timer's counter will initially start from value below.
       Also, if auto_reload is set, this value will be automatically reload on alarm */
    timer_set_counter_value(TIMER_GROUP_0, TIMER_0, 0x00000000ULL);

    /* Configure the alarm value and the interrupt on alarm. */
    timer_set_alarm_value(TIMER_GROUP_0, TIMER_0, 10000);
    timer_enable_intr(TIMER_GROUP_0, TIMER_0);
    timer_isr_register(TIMER_GROUP_0, TIMER_0, timer_group0_isr,
                       (void *)TIMER_0, ESP_INTR_FLAG_IRAM, NULL);
    timer_start(TIMER_GROUP_0, TIMER_0);
}