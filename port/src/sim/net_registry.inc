// Registration of module net (included by net.cpp, in namespace portsim). Registered under the hooks.csv names; nothing
// in tick calls them (decision G14).
namespace {
using net::g_nc_add_voice;
void a_name_of(Ctx &c, const uint32_t *a, Ret &r) { r.set_u32(net::net_name_of(c, a[0], a[1])); }
void a_begin0(Ctx &c, const uint32_t *a, Ret &r) { r.set_u32(net::net_begin_op0(c, a[0], a[1])); }
void a_begin1(Ctx &c, const uint32_t *a, Ret &r) { r.set_u32(net::net_begin_op1(c, a[0], a[1])); }
void a_flush(Ctx &c, const uint32_t *a, Ret &r) { r.set_u32(net::net_flush(c, a[0], a[1])); }
void a_pump(Ctx &c, const uint32_t *a, Ret &r) { r.set_u32(net::net_pump(c, a[0])); }
void a_send_list(Ctx &c, const uint32_t *a, Ret &) { net::net_send_list(c, a[0]); }
void a_queue(Ctx &c, const uint32_t *a, Ret &r) { r.set_u32(net::net_queue_message(c, a[0], a[1], a[2], a[3])); }
void a_list_remove(Ctx &c, const uint32_t *a, Ret &) { net::net_list_remove(c, a[0], a[1]); }
void a_poll(Ctx &c, const uint32_t *a, Ret &r) { r.set_u32(net::net_poll(c, a[0])); }
void a_svc_ctl(Ctx &c, const uint32_t *a, Ret &r) { r.set_u32(net::net_service_control(c, a[0])); }
void a_shutdown(Ctx &c, const uint32_t *a, Ret &) { net::net_send_shutdown(c, a[0]); }
void a_handle(Ctx &c, const uint32_t *a, Ret &r) { r.set_u32(net::net_handle_control(c, a[0], a[1], a[2])); }
// S3List_addVoiceRx is the ring list's add with a 0x64-byte VoiceRx payload built by the exe's operator new
// and VoiceRx::ctor chain; the port records the request at the host (U-1772).
void a_add_voice(Ctx &, const uint32_t *a, Ret &r) {
  r.set_u32(SG_NEGCTL(net::g_nc_add_voice) == 1 ? 0 : net::g_host->call(net::Call::ListAddVoiceRx, {a[0], a[1]}));
}

const FuncDef kFuncs[] = {
    {"netNameOf", "thiscall", "pointer", 2, 0, a_name_of, &net::g_nc_name_of},
    {"netBeginOp0", "thiscall", "int", 2, 0, a_begin0, &net::g_nc_begin0},
    {"netBeginOp1", "thiscall", "int", 2, 0, a_begin1, &net::g_nc_begin1},
    {"netFlush", "thiscall", "int", 2, 0, a_flush, &net::g_nc_flush},
    {"netPump", "thiscall", "uint32", 1, 0, a_pump, &net::g_nc_pump},
    {"netSendList", "thiscall", "void", 1, 0, a_send_list, &net::g_nc_send_list},
    {"netQueueMessage", "thiscall", "uint32", 4, 0, a_queue, &net::g_nc_queue},
    {"netListRemove", "thiscall", "void", 2, 0, a_list_remove, &net::g_nc_list_remove},
    {"netPoll", "thiscall", "uint32", 1, 0, a_poll, &net::g_nc_poll},
    {"netServiceControl", "thiscall", "uint32", 1, 0, a_svc_ctl, &net::g_nc_svc_ctl},
    {"netSendShutdown", "thiscall", "void", 1, 0, a_shutdown, &net::g_nc_shutdown},
    {"netHandleControl", "thiscall", "int", 3, 0, a_handle, &net::g_nc_handle},
    {"S3List_addVoiceRx", "thiscall", "int", 2, 0, a_add_voice, &net::g_nc_add_voice},
    {"net.hostReset", "default", "void", 0, 0, net::t_reset, nullptr},
    {"net.hostAnswer", "default", "void", 2, 0, net::t_answer, nullptr},
    {"net.hostPush", "default", "void", 3, 0, net::t_push, nullptr},
    {"net.hostLogCount", "default", "uint32", 0, 0, net::t_count, nullptr},
    {"net.hostLogCall", "default", "uint32", 1, 0, net::t_call, nullptr},
    {"net.hostLogArgc", "default", "uint32", 1, 0, net::t_argc, nullptr},
    {"net.hostLogArg", "default", "uint32", 2, 0, net::t_arg, nullptr},
    {"net.hostPayloadLen", "default", "uint32", 1, 0, net::t_plen, nullptr},
    {"net.hostPayloadByte", "default", "uint32", 2, 0, net::t_pbyte, nullptr},
};
const DefectDef kDefects[] = {
    {"netHandleControl.slot15Stays", "NET_MODEL.md 9.2", "slot 15 keeps its old contents after a peer is removed",
     &net::g_defect_slot15},
};
}  // namespace

extern const ModuleDef kModule_net = {"net", kFuncs, sizeof(kFuncs) / sizeof(kFuncs[0]), kDefects,
                                      sizeof(kDefects) / sizeof(kDefects[0])};
