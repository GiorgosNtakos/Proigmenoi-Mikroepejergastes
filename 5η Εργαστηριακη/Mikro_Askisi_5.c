
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdint.h>

/*
 * Εργαστηριακή Άσκηση 5 – Έξυπνος αυτοματισμός σπιτιού (ATmega4808)
 *
 *   LEDs: LED0..LED3 -> PD0..PD3
 *   Switches: SW5 -> PC5, SW6 -> PF5 (pull-up, press = falling)
 *
 * Σημείωση:
 *   Η εκφώνηση λέει ότι δεν χρειάζεται να υλοποιηθεί το πάτημα των
 *   switches με interrupt. Εδώ τα pins έχουν ISC_FALLING ώστε να παράγουν
 *   INTFLAGS, και το πρόγραμμα κάνει polling στα flags (χωρίς ISR).
 */

/* ------------------- User-tunable parameters ------------------- */
#define THERMO_TIMEOUT_CMP   (40u)  // "μεγάλος" χρόνος για θερμοσίφωνα (με prescaler 1024)
#define LOCK_TIMEOUT_CMP     (20u)  // χρονικό περιθώριο για εισαγωγή κωδικού

#define ADC_TAMPER_LOW       (120u)    // κάτω από αυτό -> "παραβίαση" (LED3)
#define ADC_LEAK_HIGH        (850u)    // πάνω από αυτό -> "διαρροή" (LED2)

/* Fan software PWM (toggle LED1 on each interrupt => blink) */
#define FAN_TCB_CCMP         (10u)   // μικρότερη τιμή => πιο γρήγορο blink

/* Code sequence for unlock: SW5-SW6-SW5 (όπως στο παράδειγμα του φυλλαδίου) */
#define CODE_LEN 3
static const uint8_t unlock_code[CODE_LEN] = { 5, 6, 5 };

/* ------------------- LED convenience ------------------- */
#define LED0_bm PIN0_bm
#define LED1_bm PIN1_bm
#define LED2_bm PIN2_bm
#define LED3_bm PIN3_bm

typedef enum {
    TCA_MODE_NONE = 0,
    TCA_MODE_THERMO,
    TCA_MODE_LOCK_TIMEOUT
} tca_mode_t;

/* ------------------- Global state ------------------- */
volatile uint8_t thermo_on = 0;
volatile uint8_t fan_on    = 0;
volatile uint8_t leak_lit  = 0;

volatile uint8_t thermo_need_decision = 0;

volatile uint8_t lock_required = 0;     // πρέπει να μπει κωδικός (από tamper ή από manual lock)
volatile uint8_t lock_from_tamper = 0;   // 1 όταν το lock ξεκίνησε από ADC tamper
volatile uint8_t permanent_lock = 0;    // LED3 ON, καμία άλλη ενέργεια

volatile uint8_t lock_timeout_fired = 0;

volatile tca_mode_t tca_mode = TCA_MODE_NONE;

/* ------------------- Forward declarations ------------------- */
static void init_leds(void);
static void init_switches(void);
static void init_adc_outside_window(void);
static void init_tca0_base(void);
static void init_fan_tcb0(void);

static inline uint8_t sw5_event(void);
static inline uint8_t sw6_event(void);

static void tca0_start_cmp0(uint16_t cmp, tca_mode_t mode);
static void tca0_stop(void);

static void device_thermosifonas(void);
static void device_fan(void);
static void device_water_leak_ack(void);
static void device_lock_house(void);

static void handle_thermo_interrupt_modal(void);
static void enter_unlock_sequence(uint8_t led3_on_already);
static void stop_everything_and_clear_leds012(void);
static void restore_normal_runtime_defaults(void);

/* ------------------- Switch polling helpers ------------------- */
static inline uint8_t sw5_event(void)
{
    /* SW5 is on PC5 */
    if (PORTC.INTFLAGS & PIN5_bm) {
        PORTC.INTFLAGS = PIN5_bm; // clear
        return 1;
    }
    return 0;
}

static inline uint8_t sw6_event(void)
{
    /* SW6 is on PF5 */
    if (PORTF.INTFLAGS & PIN5_bm) {
        PORTF.INTFLAGS = PIN5_bm; // clear
        return 1;
    }
    return 0;
}

/* ------------------- Peripheral init ------------------- */
static void init_leds(void)
{
    PORTD.DIR |= LED0_bm | LED1_bm | LED2_bm | LED3_bm;
    PORTD.OUTCLR = LED0_bm | LED1_bm | LED2_bm | LED3_bm;
}

static void init_switches(void)
{
    /* pull-up + falling edge -> sets INTFLAGS on press (we poll flags) */
    PORTC.PIN5CTRL = PORT_PULLUPEN_bm | PORT_ISC_FALLING_gc; // SW5
    PORTF.PIN5CTRL = PORT_PULLUPEN_bm | PORT_ISC_FALLING_gc; // SW6

    /* clear any stale flags */
    PORTC.INTFLAGS = PIN5_bm;
    PORTF.INTFLAGS = PIN5_bm;
}

static void init_adc_outside_window(void)
{
    /* 10-bit, freerun, enable */
    ADC0.CTRLA  = ADC_RESSEL_10BIT_gc | ADC_FREERUN_bm | ADC_ENABLE_bm;
    ADC0.MUXPOS = ADC_MUXPOS_AIN7_gc;                 // όπως στην άσκηση 3
    ADC0.DBGCTRL = ADC_DBGRUN_bm;

    /* window thresholds */
    ADC0.WINLT = ADC_TAMPER_LOW;
    ADC0.WINHT = ADC_LEAK_HIGH;

    /* interrupt on window comparator */
    ADC0.INTCTRL = ADC_WCMP_bm;

    /* OUTSIDE window: interrupt when RES < WINLT OR RES > WINHT */
    ADC0.CTRLE = ADC_WINCM_OUTSIDE_gc;

    /* start conversions */
    ADC0.COMMAND = ADC_STCONV_bm;
}

static void init_tca0_base(void)
{
    TCA0.SINGLE.CNT   = 0;
    TCA0.SINGLE.CTRLB = 0; // normal mode
    TCA0.SINGLE.CTRLA = TCA_SINGLE_CLKSEL_DIV1024_gc; // base prescaler
    TCA0.SINGLE.INTCTRL = TCA_SINGLE_CMP0_bm;
    //TCA0.SINGLE.INTFLAGS = TCA_SINGLE_CMP0_bm;
}

static void init_fan_tcb0(void)
{
    /* TCB0 periodic interrupt (software PWM / blink) */
    TCB0.CTRLA = 0; // stop
    TCB0.CTRLB = TCB_CNTMODE_INT_gc; // periodic interrupt mode
    TCB0.CCMP  = FAN_TCB_CCMP;
    TCB0.INTCTRL = TCB_CAPT_bm;
    //TCB0.INTFLAGS = TCB_CAPT_bm;
    /* enable with CLKDIV1 */
    TCB0.CTRLA = TCB_CLKSEL_CLKDIV1_gc | TCB_ENABLE_bm;
}

/* ------------------- TCA0 helpers ------------------- */
static void tca0_start_cmp0(uint16_t cmp, tca_mode_t mode)
{
    tca_mode = mode;
    TCA0.SINGLE.CNT = 0;
    TCA0.SINGLE.CMP0 = cmp;
    TCA0.SINGLE.INTFLAGS = TCA_SINGLE_CMP0_bm;
    TCA0.SINGLE.CTRLA |= TCA_SINGLE_ENABLE_bm;
}

static void tca0_stop(void)
{
    TCA0.SINGLE.CTRLA &= ~TCA_SINGLE_ENABLE_bm;
    TCA0.SINGLE.CNT = 0;
    TCA0.SINGLE.INTFLAGS = TCA_SINGLE_CMP0_bm;
    tca_mode = TCA_MODE_NONE;
}

/* ------------------- ISRs ------------------- */
ISR(TCA0_CMP0_vect)
{
    TCA0.SINGLE.INTFLAGS = TCA_SINGLE_CMP0_bm;

    if (tca_mode == TCA_MODE_THERMO) {
        /* θερμοσίφωνας: σταμάτα timer και ζήτα απόφαση από χρήστη */
        tca0_stop();
        thermo_need_decision = 1;
    }
    else if (tca_mode == TCA_MODE_LOCK_TIMEOUT) {
        /* timeout κωδικού */
        tca0_stop();
        lock_timeout_fired = 1;
    }
}

ISR(TCB0_INT_vect)
{
    /* clear flag */
    TCB0.INTFLAGS = TCB_CAPT_bm;

    /* "PWM" ανεμιστήρα: toggle LED1 σε κάθε interrupt */
    if (fan_on && !permanent_lock && !lock_required) {
        PORTD.OUTTGL = LED1_bm;
    }
}

ISR(ADC0_WCOMP_vect)
{
    ADC0.INTFLAGS = ADC_WCMP_bm;

    if (permanent_lock) return;

    uint16_t res = ADC0.RES;

    /* Διαρροή νερού: RES > WINHT */
    if (res > ADC_LEAK_HIGH) {
        leak_lit = 1;
        PORTD.OUTSET = LED2_bm;
        return;
    }

    /* Παραβίαση: RES < WINLT */
    if (res < ADC_TAMPER_LOW) {
        /* ενεργοποίησε διαδικασία "κλειδώματος" */
        lock_required = 1;
        lock_from_tamper = 1;
        PORTD.OUTSET = LED3_bm;
        lock_timeout_fired = 0;
        /* ξεκίνα timeout για εισαγωγή κωδικού */
        tca0_start_cmp0(LOCK_TIMEOUT_CMP, TCA_MODE_LOCK_TIMEOUT);
    }
}

/* ------------------- Device functions ------------------- */
static void device_thermosifonas(void)
{
    /* επιλογή: on/off */
    if (!thermo_on) {
        thermo_on = 1;
        PORTD.OUTSET = LED0_bm;
        /* ξεκίνα timer για "πολύ ώρα" */
        thermo_need_decision = 0;
        tca0_start_cmp0(THERMO_TIMEOUT_CMP, TCA_MODE_THERMO);
    } else {
        thermo_on = 0;
        PORTD.OUTCLR = LED0_bm;
        /* αν ήταν σε εξέλιξη timer θερμοσίφωνα, σταμάτα τον */
        if (tca_mode == TCA_MODE_THERMO) tca0_stop();
        thermo_need_decision = 0;
    }
}

static void device_fan(void)
{
    if (!fan_on) {
        fan_on = 1;
        /* αρχική κατάσταση LED1 off -> θα αρχίσει να αναβοσβήνει από ISR */
        PORTD.OUTCLR = LED1_bm;
    } else {
        fan_on = 0;
        PORTD.OUTCLR = LED1_bm;
    }
}

static void device_water_leak_ack(void)
{
    /* ο χρήστης δηλώνει ότι είδε την ειδοποίηση */
    leak_lit = 0;
    PORTD.OUTCLR = LED2_bm;
}

static void device_lock_house(void)
{
    /* Manual lock: απενεργοποίηση όλων & απαίτηση κωδικού */
    lock_required = 1;
    lock_from_tamper = 0;

    /* Η εκφώνηση θέλει να σβήνουν τα LED0..LED2 όταν κλειδώνει */
    stop_everything_and_clear_leds012();

    /* LED3 δεν είναι απαραίτητο να ανάψει σε manual lock, αλλά δεν απαγορεύεται.
       Το ανάβουμε μόνο σε tamper/3 λάθη/timeout. */

    /* Χρονικό περιθώριο για εισαγωγή κωδικού */
    lock_timeout_fired = 0;
    tca0_start_cmp0(LOCK_TIMEOUT_CMP, TCA_MODE_LOCK_TIMEOUT);
}

/* ------------------- Lock / unlock logic ------------------- */
static void stop_everything_and_clear_leds012(void)
{
    /* stop thermo timer */
    tca0_stop();
    thermo_need_decision = 0;

    /* stop fan */
    fan_on = 0;
    PORTD.OUTCLR = LED1_bm;

    /* turn off LED0..LED2 (εκφώνηση) */
    thermo_on = 0;
    leak_lit  = 0;
    PORTD.OUTCLR = LED0_bm | LED1_bm | LED2_bm;
}

static void restore_normal_runtime_defaults(void)
{
    /* Μετά το unlock: επιστρέφουμε στην "αρχική λειτουργία" χωρίς να επαναφέρουμε
       τα προηγούμενα states (όπως ζητά η εκφώνηση). */
    thermo_on = 0;
    fan_on = 0;
    leak_lit = 0;
    thermo_need_decision = 0;

    PORTD.OUTCLR = LED0_bm | LED1_bm | LED2_bm;

    /* ADC: ξανά στο outside-window (διαρροή + tamper) */
    init_adc_outside_window();

    /* Timer base already configured. */
}

static void enter_unlock_sequence(uint8_t led3_on_already)
{
    /* Αν έχουμε permanent_lock, δεν βγαίνουμε. */
    if (permanent_lock) return;

    /* Κατά την προσπάθεια εισαγωγής συνδυασμού δεν θέλουμε interrupts.
       (όπως ζητά η εκφώνηση)
       Θα κάνουμε polling στα INTFLAGS και θα κοιτάμε και τον timeout flag. */
    cli();

    uint8_t idx = 0;
    uint8_t wrong = 0;

    /* Καθάρισε τυχόν flags από παλιά presses */
    PORTC.INTFLAGS = PIN5_bm;
    PORTF.INTFLAGS = PIN5_bm;

    while (1)
    {
        /* Αν έληξε το χρονικό περιθώριο => μόνιμο κλείδωμα */
        /* Timeout είτε από ISR (αν πρόλαβε), είτε με polling (αφού έχουμε cli()) */
        if (lock_timeout_fired || ((tca_mode == TCA_MODE_LOCK_TIMEOUT) && (TCA0.SINGLE.CNT >= TCA0.SINGLE.CMP0))) {
            permanent_lock = 1;
            PORTD.OUTSET = LED3_bm;
            break;
        }

        /* Περιμένουμε οποιοδήποτε από τα δύο switches */
        uint8_t got5 = (PORTC.INTFLAGS & PIN5_bm) ? 1 : 0;
        uint8_t got6 = (PORTF.INTFLAGS & PIN5_bm) ? 1 : 0;

        if (!got5 && !got6) {
            /* μικρό nop για να μην τρέχει πολύ γρήγορα */
            __asm__ __volatile__("nop");
            continue;
        }

        /* Προτεραιότητα: αν πατηθούν και τα δύο, το θεωρούμε λάθος */
        if (got5 && got6) {
            PORTC.INTFLAGS = PIN5_bm;
            PORTF.INTFLAGS = PIN5_bm;
            idx = 0;
            wrong++;
        }
        else if (got5) {
            PORTC.INTFLAGS = PIN5_bm;
            if (unlock_code[idx] == 5) idx++; else { idx = 0; wrong++; }
        }
        else if (got6) {
            PORTF.INTFLAGS = PIN5_bm;
            if (unlock_code[idx] == 6) idx++; else { idx = 0; wrong++; }
        }

        if (wrong >= 3) {
            permanent_lock = 1;
            PORTD.OUTSET = LED3_bm;
            break;
        }

        if (idx >= CODE_LEN) {
            /* σωστός κωδικός */
            break;
        }
    }

    sei();

    /* Σταμάτα timeout timer */
    tca0_stop();

    if (permanent_lock) {
        /* απενεργοποίησε τα πάντα */
        stop_everything_and_clear_leds012();
        return;
    }

    /* Unlock success */
    lock_required = 0;
    lock_from_tamper = 0;

    if (!led3_on_already) {
        /* σε manual lock μπορεί να μην ήταν αναμμένο */
        PORTD.OUTCLR = LED3_bm;
    }
    else {
        /* σε tamper ήταν αναμμένο και το σβήνουμε μετά το unlock */
        PORTD.OUTCLR = LED3_bm;
    }

    restore_normal_runtime_defaults();
}

/* ------------------- Thermo modal handler ------------------- */
static void handle_thermo_interrupt_modal(void)
{
    /* Όταν ο θερμοσίφωνας είναι on για "πολύ ώρα":
       - γίνεται interrupt
       - περιμένουμε μέχρι να πατηθεί switch
       - SW5 => παραμένει on και συνεχίζει ο timer
       - SW6 => off και timer off
    */

    /* Καθάρισε τυχόν flags */
    PORTC.INTFLAGS = PIN5_bm;
    PORTF.INTFLAGS = PIN5_bm;
	PORTD.OUTCLR = LED0_bm;

    while (thermo_need_decision && !permanent_lock)
    {
        if (sw5_event()) {
            /* μένει ανοιχτός -> restart timer */
            thermo_need_decision = 0;
			PORTD.OUTSET = LED0_bm;
            if (thermo_on) {
                tca0_start_cmp0(THERMO_TIMEOUT_CMP, TCA_MODE_THERMO);
            }
            break;
        }

        if (sw6_event()) {
            /* κλείνει */
            thermo_need_decision = 0;
            thermo_on = 0;
            PORTD.OUTCLR = LED0_bm;
            tca0_stop();
            break;
        }

        __asm__ __volatile__("nop");
    }
}

/* ------------------- Main (device selection loops) ------------------- */
int main(void)
{
    init_leds();
    init_switches();
    init_tca0_base();
    init_fan_tcb0();
    init_adc_outside_window();

    sei();
	
	uint8_t selected_device = 1;  // 1..4
	
    while (1)
    {
		
        if (permanent_lock) {
            PORTD.OUTSET = LED3_bm;
            while (1) { __asm__ __volatile__("nop"); }
        }

        /* 1) Αν έχει χτυπήσει timer θερμοσίφωνα, αυτό είναι modal (όπως λέει η εκφώνηση) */
        if (thermo_need_decision) {
            handle_thermo_interrupt_modal();
            continue;
        }

        /* 2) Αν απαιτείται κωδικός (tamper ή manual lock), μπαίνουμε σε unlock */
        if (lock_required) {
            /* Σε lock mode απενεργοποιούνται όλα (εκτός από LED3 αν είναι alarm). */
            stop_everything_and_clear_leds012();
            enter_unlock_sequence(1);
            lock_from_tamper = 0;
            continue;
        }

        /* 3) Επιλογή συσκευής με 4 while loops (όπως προτείνει το φυλλάδιο) */

        /* 3) Επιλογή συσκευής (menu) */
        switch (selected_device)
        {
	        case 1: /* Device 1: Θερμοσίφωνας */
	        while (!permanent_lock && !lock_required && !thermo_need_decision)
	        {
		        if (sw6_event()) { device_thermosifonas(); break; }  // break ΝΑΙ
		        if (sw5_event()) { selected_device = 2; break; }     // μόνο SW5 αλλάζει device
	        }
	        break;

	        case 2: /* Device 2: Ανεμιστήρας */
	        while (!permanent_lock && !lock_required && !thermo_need_decision)
	        {
		        if (sw6_event()) { device_fan(); break; }
		        if (sw5_event()) { selected_device = 3; break; }
	        }
	        break;

	        case 3: /* Device 3: Water leakage ACK */
	        while (!permanent_lock && !lock_required && !thermo_need_decision)
	        {
		        if (sw6_event()) { device_water_leak_ack(); break; }
		        if (sw5_event()) { selected_device = 4; break; }
	        }
	        break;

	        default: /* case 4: Lock */
	        while (!permanent_lock && !lock_required && !thermo_need_decision)
	        {
		        if (sw6_event()) { device_lock_house(); break; }
		        if (sw5_event()) { selected_device = 1; break; }
	        }
	        break;
        }

        /* Αν πάτησες SW6, selected_device ΔΕΝ αλλάζει → άρα στο επόμενο iteration θα ξαναμπείς στο ίδιο case. */
        /* Αν πάτησες SW5, selected_device άλλαξε → άρα θα μπεις στο επόμενο case. */


        /* Και μετά επιστρέφει ξανά στο 1ο while loop */
    }
}
