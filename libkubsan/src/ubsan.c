#include <stdint.h>
#include <stddef.h>
#include <stdnoreturn.h>

#include <kc/assert.h>

struct ubsan_source_location {
    const char *file_name;
    uint32_t line;
    uint32_t column;
};

struct ubsan_type_descriptor {
    uint16_t type_kind;
    uint16_t type_info;
    char type_name[];
};

struct ubsan_overflow_data {
    struct ubsan_source_location loc;
    struct ubsan_type_descriptor *type;
};

struct ubsan_shift_out_of_bounds_data {
    struct ubsan_source_location loc;
    struct ubsan_type_descriptor *lhs_type;
    struct ubsan_type_descriptor *rhs_type;
};

struct ubsan_out_of_bounds_data {
    struct ubsan_source_location loc;
    struct ubsan_type_descriptor *array_type;
    struct ubsan_type_descriptor *index_type;
};

struct ubsan_pointer_overflow_data {
    struct ubsan_source_location loc;
};

struct ubsan_type_mismatch_data {
    struct ubsan_source_location loc;
    struct ubsan_type_descriptor *type;
    unsigned char log_alignment;
    unsigned char type_check_kind;
};

struct ubsan_alignment_assumption_data {
    struct ubsan_source_location loc;
    struct ubsan_source_location assumption_loc;
    struct ubsan_type_descriptor *type;
    unsigned char log_alignment;
    unsigned char type_check_kind;
};

struct ubsan_load_invalid_value_data {
    struct ubsan_source_location loc;
    struct ubsan_type_descriptor *type;
};

struct ubsan_nonnull_arg_data {
    struct ubsan_source_location loc;
    struct ubsan_source_location attr_loc;
    int arg_index;
};

struct ubsan_nonnull_return_data {
    struct ubsan_source_location loc;
};

struct ubsan_vla_bound_data {
    struct ubsan_source_location loc;
    struct ubsan_type_descriptor *type;
};

__attribute__((no_sanitize_undefined))
static noreturn void ubsan_report(const char *kind, const struct ubsan_source_location *loc) {
    const char *file_name = "<unknown>";
    uint32_t line = 0;
    uint32_t column = 0;

    if (loc != NULL) {
        if (loc->file_name != NULL) {
            file_name = loc->file_name;
        }
        line = loc->line;
        column = loc->column;
    }

    panic_format("ubsan: %s at %s:%u:%u", __FILE__, __func__, __LINE__, kind, file_name, line, column);
}

#define UBSAN_HANDLER_LOC(name, kind, data_type) \
    __attribute__((no_sanitize_undefined)) \
    void name(data_type *data, ...) { \
        ubsan_report(kind, data != NULL ? &data->loc : NULL); \
    }

UBSAN_HANDLER_LOC(__ubsan_handle_add_overflow, "add-overflow", struct ubsan_overflow_data)
UBSAN_HANDLER_LOC(__ubsan_handle_sub_overflow, "sub-overflow", struct ubsan_overflow_data)
UBSAN_HANDLER_LOC(__ubsan_handle_mul_overflow, "mul-overflow", struct ubsan_overflow_data)
UBSAN_HANDLER_LOC(__ubsan_handle_negate_overflow, "negate-overflow", struct ubsan_overflow_data)
UBSAN_HANDLER_LOC(__ubsan_handle_divrem_overflow, "divrem-overflow", struct ubsan_overflow_data)

UBSAN_HANDLER_LOC(__ubsan_handle_shift_out_of_bounds, "shift-out-of-bounds", struct ubsan_shift_out_of_bounds_data)
UBSAN_HANDLER_LOC(__ubsan_handle_out_of_bounds, "out-of-bounds", struct ubsan_out_of_bounds_data)
UBSAN_HANDLER_LOC(__ubsan_handle_pointer_overflow, "pointer-overflow", struct ubsan_pointer_overflow_data)

UBSAN_HANDLER_LOC(__ubsan_handle_type_mismatch, "type-mismatch", struct ubsan_type_mismatch_data)
UBSAN_HANDLER_LOC(__ubsan_handle_type_mismatch_v1, "type-mismatch-v1", struct ubsan_type_mismatch_data)

UBSAN_HANDLER_LOC(__ubsan_handle_alignment_assumption, "alignment-assumption", struct ubsan_alignment_assumption_data)
UBSAN_HANDLER_LOC(__ubsan_handle_load_invalid_value, "load-invalid-value", struct ubsan_load_invalid_value_data)
UBSAN_HANDLER_LOC(__ubsan_handle_vla_bound_not_positive, "vla-bound-not-positive", struct ubsan_vla_bound_data)

__attribute__((no_sanitize_undefined))
void __ubsan_handle_nonnull_arg(struct ubsan_nonnull_arg_data *data) {
    ubsan_report("nonnull-arg", data != NULL ? &data->loc : NULL);
}

__attribute__((no_sanitize_undefined))
void __ubsan_handle_nonnull_return(struct ubsan_nonnull_return_data *data) {
    ubsan_report("nonnull-return", data != NULL ? &data->loc : NULL);
}

__attribute__((no_sanitize_undefined))
void __ubsan_handle_nonnull_return_v1(struct ubsan_nonnull_return_data *data, struct ubsan_source_location *loc) {
    (void)data;
    ubsan_report("nonnull-return-v1", loc);
}

__attribute__((no_sanitize_undefined))
void __ubsan_handle_builtin_unreachable(struct ubsan_source_location *loc) {
    ubsan_report("builtin-unreachable", loc);
}

__attribute__((no_sanitize_undefined))
void __ubsan_handle_missing_return(struct ubsan_source_location *loc) {
    ubsan_report("missing-return", loc);
}

/* abort variants */
UBSAN_HANDLER_LOC(__ubsan_handle_add_overflow_abort, "add-overflow", struct ubsan_overflow_data)
UBSAN_HANDLER_LOC(__ubsan_handle_sub_overflow_abort, "sub-overflow", struct ubsan_overflow_data)
UBSAN_HANDLER_LOC(__ubsan_handle_mul_overflow_abort, "mul-overflow", struct ubsan_overflow_data)
UBSAN_HANDLER_LOC(__ubsan_handle_negate_overflow_abort, "negate-overflow", struct ubsan_overflow_data)
UBSAN_HANDLER_LOC(__ubsan_handle_divrem_overflow_abort, "divrem-overflow", struct ubsan_overflow_data)
UBSAN_HANDLER_LOC(__ubsan_handle_shift_out_of_bounds_abort, "shift-out-of-bounds", struct ubsan_shift_out_of_bounds_data)
UBSAN_HANDLER_LOC(__ubsan_handle_out_of_bounds_abort, "out-of-bounds", struct ubsan_out_of_bounds_data)
UBSAN_HANDLER_LOC(__ubsan_handle_pointer_overflow_abort, "pointer-overflow", struct ubsan_pointer_overflow_data)
UBSAN_HANDLER_LOC(__ubsan_handle_type_mismatch_abort, "type-mismatch", struct ubsan_type_mismatch_data)
UBSAN_HANDLER_LOC(__ubsan_handle_type_mismatch_v1_abort, "type-mismatch-v1", struct ubsan_type_mismatch_data)
UBSAN_HANDLER_LOC(__ubsan_handle_alignment_assumption_abort, "alignment-assumption", struct ubsan_alignment_assumption_data)
UBSAN_HANDLER_LOC(__ubsan_handle_load_invalid_value_abort, "load-invalid-value", struct ubsan_load_invalid_value_data)
UBSAN_HANDLER_LOC(__ubsan_handle_vla_bound_not_positive_abort, "vla-bound-not-positive", struct ubsan_vla_bound_data)

/* Optional handlers emitted by newer compilers. */
__attribute__((no_sanitize_undefined))
void __ubsan_handle_float_cast_overflow(struct ubsan_source_location *loc, ...) {
    ubsan_report("float-cast-overflow", loc);
}

__attribute__((no_sanitize_undefined))
void __ubsan_handle_float_cast_overflow_abort(struct ubsan_source_location *loc, ...) {
    ubsan_report("float-cast-overflow", loc);
}

__attribute__((no_sanitize_undefined))
void __ubsan_handle_implicit_conversion(struct ubsan_source_location *loc, ...) {
    ubsan_report("implicit-conversion", loc);
}

__attribute__((no_sanitize_undefined))
void __ubsan_handle_implicit_conversion_abort(struct ubsan_source_location *loc, ...) {
    ubsan_report("implicit-conversion", loc);
}
