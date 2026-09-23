#ifndef CPKT_GSSAPI_H
#define CPKT_GSSAPI_H

/**
 * @defgroup cpkt_gssapi GSSAPI C89 facade
 *
 * Opaque names, credentials, contexts, OIDs, and OID sets returned as owned
 * results use their matching release operation. Standard name-type OIDs and
 * OID-set members are borrowed. Output buffers are provider-owned until
 * cpkt_gss_release_buffer(). See docs/gssapi-c89-facade-spec.md.
 * @{
 */

#include <stddef.h>

/** Opaque GSS name; release owned instances with cpkt_gss_release_name(). */
typedef struct cpkt_gss_name cpkt_gss_name;
/** C89 GSSAPI facade declaration. See docs/gssapi-c89-facade-spec.md. */
typedef struct cpkt_gss_credential cpkt_gss_credential;
/** C89 GSSAPI facade declaration. See docs/gssapi-c89-facade-spec.md. */
typedef struct cpkt_gss_context cpkt_gss_context;
/** C89 GSSAPI facade declaration. See docs/gssapi-c89-facade-spec.md. */
typedef struct cpkt_gss_oid cpkt_gss_oid;
/** C89 GSSAPI facade declaration. See docs/gssapi-c89-facade-spec.md. */
typedef struct cpkt_gss_oid_set cpkt_gss_oid_set;

/* GSSAPI status, QOP, lifetime, and flag values are exact unsigned 32-bit
 * values carried in an unsigned long on every supported target. */
/** C89 GSSAPI facade declaration. See docs/gssapi-c89-facade-spec.md. */
typedef unsigned long cpkt_gss_status;
/** C89 GSSAPI facade declaration. See docs/gssapi-c89-facade-spec.md. */
typedef unsigned long cpkt_gss_qop;
/** C89 GSSAPI facade declaration. See docs/gssapi-c89-facade-spec.md. */
typedef unsigned long cpkt_gss_lifetime;
/** C89 GSSAPI facade declaration. See docs/gssapi-c89-facade-spec.md. */
typedef unsigned long cpkt_gss_flags;

/** Provider-owned output byte buffer; release with cpkt_gss_release_buffer().
 */
typedef struct cpkt_gss_buffer {
  size_t length;
  void *value;
} cpkt_gss_buffer;

/** C89 GSSAPI facade declaration. See docs/gssapi-c89-facade-spec.md. */
typedef struct cpkt_gss_channel_bindings {
  unsigned long initiator_address_type;
  cpkt_gss_buffer initiator_address;
  unsigned long acceptor_address_type;
  cpkt_gss_buffer acceptor_address;
  cpkt_gss_buffer application_data;
} cpkt_gss_channel_bindings;

enum cpkt_gss_constants {
  CPKT_GSS_COMPLETE = 0,
  CPKT_GSS_CREDENTIAL_BOTH = 0,
  CPKT_GSS_CREDENTIAL_INITIATE = 1,
  CPKT_GSS_CREDENTIAL_ACCEPT = 2,
  CPKT_GSS_STATUS_GSS_CODE = 1,
  CPKT_GSS_STATUS_MECHANISM_CODE = 2,
  CPKT_GSS_QOP_DEFAULT = 0,
  CPKT_GSS_FLAG_DELEGATE = 1,
  CPKT_GSS_FLAG_MUTUAL = 2,
  CPKT_GSS_FLAG_REPLAY = 4,
  CPKT_GSS_FLAG_SEQUENCE = 8,
  CPKT_GSS_FLAG_CONFIDENTIALITY = 16,
  CPKT_GSS_FLAG_INTEGRITY = 32,
  CPKT_GSS_FLAG_ANONYMOUS = 64,
  CPKT_GSS_FLAG_PROTECTION_READY = 128,
  CPKT_GSS_FLAG_TRANSFERRABLE = 256
};

#define CPKT_GSS_LIFETIME_INDEFINITE 4294967295UL

/** Returns nonzero when the major status contains a GSSAPI error. */
int cpkt_gss_status_is_error(cpkt_gss_status status);
/** C89 GSSAPI facade declaration. See docs/gssapi-c89-facade-spec.md. */
const cpkt_gss_oid *cpkt_gss_name_type_user(void);
/** C89 GSSAPI facade declaration. See docs/gssapi-c89-facade-spec.md. */
const cpkt_gss_oid *cpkt_gss_name_type_machine_uid(void);
/** C89 GSSAPI facade declaration. See docs/gssapi-c89-facade-spec.md. */
const cpkt_gss_oid *cpkt_gss_name_type_string_uid(void);
/** C89 GSSAPI facade declaration. See docs/gssapi-c89-facade-spec.md. */
const cpkt_gss_oid *cpkt_gss_name_type_hostbased_service(void);
/** C89 GSSAPI facade declaration. See docs/gssapi-c89-facade-spec.md. */
const cpkt_gss_oid *cpkt_gss_name_type_anonymous(void);
/** C89 GSSAPI facade declaration. See docs/gssapi-c89-facade-spec.md. */
const cpkt_gss_oid *cpkt_gss_name_type_exported_name(void);

/** Releases provider-owned bytes and clears the buffer on success. */
cpkt_gss_status cpkt_gss_release_buffer(cpkt_gss_status *minor_status_out,
                                        cpkt_gss_buffer *buffer);
/** C89 GSSAPI facade declaration. See docs/gssapi-c89-facade-spec.md. */
cpkt_gss_status cpkt_gss_release_oid(cpkt_gss_status *minor_status_out,
                                     cpkt_gss_oid **oid_in_out);
/** C89 GSSAPI facade declaration. See docs/gssapi-c89-facade-spec.md. */
cpkt_gss_status cpkt_gss_release_oid_set(cpkt_gss_status *minor_status_out,
                                         cpkt_gss_oid_set **set_in_out);
/** C89 GSSAPI facade declaration. See docs/gssapi-c89-facade-spec.md. */
cpkt_gss_status cpkt_gss_release_name(cpkt_gss_status *minor_status_out,
                                      cpkt_gss_name **name_in_out);
/** C89 GSSAPI facade declaration. See docs/gssapi-c89-facade-spec.md. */
cpkt_gss_status
cpkt_gss_release_credential(cpkt_gss_status *minor_status_out,
                            cpkt_gss_credential **credential_in_out);
/** C89 GSSAPI facade declaration. See docs/gssapi-c89-facade-spec.md. */
cpkt_gss_status cpkt_gss_delete_context(cpkt_gss_status *minor_status_out,
                                        cpkt_gss_context **context_in_out,
                                        cpkt_gss_buffer *output_token);

/** C89 GSSAPI facade declaration. See docs/gssapi-c89-facade-spec.md. */
cpkt_gss_status cpkt_gss_indicate_mechanisms(cpkt_gss_status *minor_status_out,
                                             cpkt_gss_oid_set **set_out);
/** C89 GSSAPI facade declaration. See docs/gssapi-c89-facade-spec.md. */
size_t cpkt_gss_oid_set_count(const cpkt_gss_oid_set *set);
/** C89 GSSAPI facade declaration. See docs/gssapi-c89-facade-spec.md. */
const cpkt_gss_oid *cpkt_gss_oid_set_at(const cpkt_gss_oid_set *set,
                                        size_t index);
/** C89 GSSAPI facade declaration. See docs/gssapi-c89-facade-spec.md. */
cpkt_gss_status cpkt_gss_create_oid_set(cpkt_gss_status *minor_status_out,
                                        cpkt_gss_oid_set **set_out);
/** C89 GSSAPI facade declaration. See docs/gssapi-c89-facade-spec.md. */
cpkt_gss_status cpkt_gss_add_oid_to_set(cpkt_gss_status *minor_status_out,
                                        const cpkt_gss_oid *oid,
                                        cpkt_gss_oid_set **set_in_out);
/** C89 GSSAPI facade declaration. See docs/gssapi-c89-facade-spec.md. */
cpkt_gss_status cpkt_gss_oid_set_contains(cpkt_gss_status *minor_status_out,
                                          const cpkt_gss_oid *oid,
                                          const cpkt_gss_oid_set *set,
                                          int *present_out);
/** C89 GSSAPI facade declaration. See docs/gssapi-c89-facade-spec.md. */
cpkt_gss_status cpkt_gss_oid_from_text(cpkt_gss_status *minor_status_out,
                                       const cpkt_gss_buffer *text,
                                       cpkt_gss_oid **oid_out);
/** C89 GSSAPI facade declaration. See docs/gssapi-c89-facade-spec.md. */
cpkt_gss_status cpkt_gss_oid_to_text(cpkt_gss_status *minor_status_out,
                                     const cpkt_gss_oid *oid,
                                     cpkt_gss_buffer *text_out);

/** C89 GSSAPI facade declaration. See docs/gssapi-c89-facade-spec.md. */
cpkt_gss_status cpkt_gss_import_name(cpkt_gss_status *minor_status_out,
                                     const cpkt_gss_buffer *input,
                                     const cpkt_gss_oid *name_type,
                                     cpkt_gss_name **name_out);
/** C89 GSSAPI facade declaration. See docs/gssapi-c89-facade-spec.md. */
cpkt_gss_status cpkt_gss_display_name(cpkt_gss_status *minor_status_out,
                                      const cpkt_gss_name *name,
                                      cpkt_gss_buffer *text_out,
                                      const cpkt_gss_oid **name_type_out);
/** C89 GSSAPI facade declaration. See docs/gssapi-c89-facade-spec.md. */
cpkt_gss_status cpkt_gss_compare_names(cpkt_gss_status *minor_status_out,
                                       const cpkt_gss_name *left,
                                       const cpkt_gss_name *right,
                                       int *equal_out);
/** C89 GSSAPI facade declaration. See docs/gssapi-c89-facade-spec.md. */
cpkt_gss_status cpkt_gss_duplicate_name(cpkt_gss_status *minor_status_out,
                                        const cpkt_gss_name *source,
                                        cpkt_gss_name **copy_out);
/** C89 GSSAPI facade declaration. See docs/gssapi-c89-facade-spec.md. */
cpkt_gss_status cpkt_gss_canonicalize_name(cpkt_gss_status *minor_status_out,
                                           const cpkt_gss_name *source,
                                           const cpkt_gss_oid *mechanism,
                                           cpkt_gss_name **canonical_out);
/** C89 GSSAPI facade declaration. See docs/gssapi-c89-facade-spec.md. */
cpkt_gss_status cpkt_gss_export_name(cpkt_gss_status *minor_status_out,
                                     const cpkt_gss_name *name,
                                     cpkt_gss_buffer *token_out);

/** C89 GSSAPI facade declaration. See docs/gssapi-c89-facade-spec.md. */
cpkt_gss_status cpkt_gss_acquire_credential(
    cpkt_gss_status *minor_status_out, const cpkt_gss_name *desired_name,
    cpkt_gss_lifetime requested_lifetime,
    const cpkt_gss_oid_set *desired_mechanisms, int usage,
    cpkt_gss_credential **credential_out,
    cpkt_gss_oid_set **actual_mechanisms_out, cpkt_gss_lifetime *lifetime_out);
/** C89 GSSAPI facade declaration. See docs/gssapi-c89-facade-spec.md. */
cpkt_gss_status cpkt_gss_inquire_credential(
    cpkt_gss_status *minor_status_out, const cpkt_gss_credential *credential,
    cpkt_gss_name **name_out, cpkt_gss_lifetime *lifetime_out, int *usage_out,
    cpkt_gss_oid_set **mechanisms_out);

/** C89 GSSAPI facade declaration. See docs/gssapi-c89-facade-spec.md. */
cpkt_gss_status cpkt_gss_init_context(
    cpkt_gss_status *minor_status_out, const cpkt_gss_credential *credential,
    cpkt_gss_context **context_in_out, const cpkt_gss_name *target,
    const cpkt_gss_oid *mechanism, cpkt_gss_flags requested_flags,
    cpkt_gss_lifetime requested_lifetime,
    const cpkt_gss_channel_bindings *channel_bindings,
    const cpkt_gss_buffer *input_token,
    const cpkt_gss_oid **actual_mechanism_out, cpkt_gss_buffer *output_token,
    cpkt_gss_flags *returned_flags_out, cpkt_gss_lifetime *lifetime_out);
/** C89 GSSAPI facade declaration. See docs/gssapi-c89-facade-spec.md. */
cpkt_gss_status cpkt_gss_accept_context(
    cpkt_gss_status *minor_status_out, cpkt_gss_context **context_in_out,
    const cpkt_gss_credential *credential, const cpkt_gss_buffer *input_token,
    const cpkt_gss_channel_bindings *channel_bindings,
    cpkt_gss_name **source_out, const cpkt_gss_oid **mechanism_out,
    cpkt_gss_buffer *output_token, cpkt_gss_flags *returned_flags_out,
    cpkt_gss_lifetime *lifetime_out,
    cpkt_gss_credential **delegated_credential_out);
/** C89 GSSAPI facade declaration. See docs/gssapi-c89-facade-spec.md. */
cpkt_gss_status cpkt_gss_context_lifetime(cpkt_gss_status *minor_status_out,
                                          const cpkt_gss_context *context,
                                          cpkt_gss_lifetime *lifetime_out);

/** C89 GSSAPI facade declaration. See docs/gssapi-c89-facade-spec.md. */
cpkt_gss_status cpkt_gss_get_mic(cpkt_gss_status *minor_status_out,
                                 const cpkt_gss_context *context,
                                 cpkt_gss_qop qop,
                                 const cpkt_gss_buffer *message,
                                 cpkt_gss_buffer *token_out);
/** C89 GSSAPI facade declaration. See docs/gssapi-c89-facade-spec.md. */
cpkt_gss_status cpkt_gss_verify_mic(cpkt_gss_status *minor_status_out,
                                    const cpkt_gss_context *context,
                                    const cpkt_gss_buffer *message,
                                    const cpkt_gss_buffer *token,
                                    cpkt_gss_qop *qop_out);
/** C89 GSSAPI facade declaration. See docs/gssapi-c89-facade-spec.md. */
cpkt_gss_status cpkt_gss_wrap(cpkt_gss_status *minor_status_out,
                              const cpkt_gss_context *context,
                              int confidentiality_requested, cpkt_gss_qop qop,
                              const cpkt_gss_buffer *input,
                              int *confidentiality_out,
                              cpkt_gss_buffer *output_out);
/** C89 GSSAPI facade declaration. See docs/gssapi-c89-facade-spec.md. */
cpkt_gss_status cpkt_gss_unwrap(cpkt_gss_status *minor_status_out,
                                const cpkt_gss_context *context,
                                const cpkt_gss_buffer *input,
                                cpkt_gss_buffer *output_out,
                                int *confidentiality_out,
                                cpkt_gss_qop *qop_out);
/** C89 GSSAPI facade declaration. See docs/gssapi-c89-facade-spec.md. */
cpkt_gss_status cpkt_gss_display_status(cpkt_gss_status *minor_status_out,
                                        cpkt_gss_status status, int status_type,
                                        const cpkt_gss_oid *mechanism,
                                        cpkt_gss_status *message_context_in_out,
                                        cpkt_gss_buffer *text_out);

/** @} */
#endif
