#include "MSL_Common/locale_def.h"

static const char empty_str[] = "";
static const char dot_str[] = ".";

struct lconv __lconv = {
    (char *)dot_str,    /* decimal_point */
    (char *)empty_str,  /* thousands_sep */
    (char *)empty_str,  /* grouping */
    (char *)empty_str,  /* mon_decimal_point */
    (char *)empty_str,  /* mon_thousands_sep */
    (char *)empty_str,  /* mon_grouping */
    (char *)empty_str,  /* positive_sign */
    (char *)empty_str,  /* negative_sign */
    (char *)empty_str,  /* currency_symbol */
    (char)0xFF,         /* frac_digits */
    (char)0xFF,         /* p_cs_precedes */
    (char)0xFF,         /* n_cs_precedes */
    (char)0xFF,         /* p_sep_by_space */
    (char)0xFF,         /* n_sep_by_space */
    (char)0xFF,         /* p_sign_posn */
    (char)0xFF,         /* n_sign_posn */
    (char *)empty_str,  /* int_curr_symbol */
    (char)0xFF,         /* int_frac_digits */
    (char)0xFF,         /* int_p_cs_precedes */
    (char)0xFF,         /* int_n_cs_precedes */
    (char)0xFF,         /* int_p_sep_by_space */
    (char)0xFF,         /* int_n_sep_by_space */
    (char)0xFF,         /* int_p_sign_posn */
    (char)0xFF,         /* int_n_sign_posn */
};

struct __locale _current_locale = {
    NULL,   /* next_locale */
    "C",    /* locale_name */
    NULL,   /* coll_cmpt_ptr */
    NULL,   /* ctype_cmpt_ptr */
    NULL,   /* mon_cmpt_ptr */
    NULL,   /* num_cmpt_ptr */
    NULL,   /* time_cmpt_ptr */
};
