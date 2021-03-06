#include <sstream>
#include "NodeIpmi.hpp"
#include "NodeIpmiUser.hpp"
extern "C" {
#include "ipmitool/ipmi_chassis.h"
#include "ipmitool/ipmi_user.h"
#include "lanplus.h"
int ipmi_get_user_access(struct ipmi_intf *intf, uint8_t channel_number, uint8_t user_id, struct user_access_rsp *user_access);
int ipmi_get_user_name(struct ipmi_intf *intf, uint8_t user_id, char *user_name);
}
V8_POST_TYPE(NodeIpmi);

using namespace node;
using namespace v8;

NodeIpmi::NodeIpmi(const char *interface_name) {
    interface = ipmi_intf_load((char *)interface_name);

    if (interface == NULL) {
        std::stringstream msg;
        msg << "Error loading interface named \"" << interface_name << "\"";
        V8_THROW(v8u::Err(msg.str().c_str()));
    }
    power = new NodeIpmiPower(interface);
}

NodeIpmi::~NodeIpmi() {
    delete power;
    interface->close(interface);
}

V8_EGET(NodeIpmi, GetPower) {
    NodeIpmi* self = Unwrap(info.Holder());
    V8_RET(self->power->Wrapped());
} V8_GET_END()

V8_EGET(NodeIpmi, GetHostname) {
    NodeIpmi* self = Unwrap(info.Holder());
    V8_RET(String::New((const char *)self->interface->session->hostname));
} V8_GET_END()

V8_ESET(NodeIpmi, SetHostname) {
    if (!value->IsString()) {
        V8_THROW(v8u::Err("hostname must be a string"));
    }

    NodeIpmi* self = Unwrap(info.Holder());
    String::Utf8Value strval(value->ToString());
    ipmi_intf_session_set_hostname(self->interface, *strval);
} V8_SET_END()

V8_EGET(NodeIpmi, GetUsername) {
    NodeIpmi* self = Unwrap(info.Holder());
    V8_RET(String::New((const char *)self->interface->session->username));
} V8_GET_END()

V8_ESET(NodeIpmi, SetUsername) {
    if (!value->IsString()) {
        V8_THROW(v8u::Err("username must be a string"));
    }

    NodeIpmi* self = Unwrap(info.Holder());
    String::Utf8Value strval(value->ToString());
    ipmi_intf_session_set_username(self->interface, *strval);
} V8_SET_END()

V8_EGET(NodeIpmi, GetPassword) {
    NodeIpmi* self = Unwrap(info.Holder());
    V8_RET(String::New((const char *)self->interface->session->authcode));
} V8_GET_END()

V8_ESET(NodeIpmi, SetPassword) {
    if (!value->IsString()) {
        V8_THROW(v8u::Err("password must be a string"));
    }

    NodeIpmi* self = Unwrap(info.Holder());
    String::Utf8Value strval(value->ToString());
    ipmi_intf_session_set_password(self->interface, *strval);
} V8_SET_END()

V8_EGET(NodeIpmi, GetBootdev) {
    NodeIpmi* self = Unwrap(info.Holder());
    /*
    //ipmi_chassis_get_bootparam(IPMI_CHASSIS_BOOTPARAM_BOOT_FLAGS
    // this logic is copied from ipmitool
    switch( ((rsp->data[3]>>2)&0x0f))
    {  
    case 0: printf("No override\n"); break;
    case 1: printf("Force PXE\n"); break;
    case 2: printf("Force Boot from default Hard-Drive\n"); break;
    case 3: printf("Force Boot from default Hard-Drive, request Safe-Mode\n"); break;
    case 4: printf("Force Boot from Diagnostic Partition\n"); break;
    case 5: printf("Force Boot from CD/DVD\n"); break;
    case 6: printf("Force Boot into BIOS Setup\n"); break;
    case 15: printf("Force Boot from Floppy/primary removable media\n"); break;
    default: printf("Flag error\n"); break;
    }
     */
    V8_RET(String::New("TODO"));
} V8_GET_END()

V8_ESET(NodeIpmi, SetBootdev) {
    // TODO maybe this should be wrapped in a .js module itself to check for
    // membership of this implicit enum type. I really don't feel like doing
    // that in C...
    if (!value->IsString()) {
        V8_THROW(v8u::Err("bootdev must be a string, one of: none, pxe, disk, safe, diag, cdrom, floppy, bios"));
    }

    NodeIpmi* self = Unwrap(info.Holder());
    String::Utf8Value strval(value->ToString());
    char *argv[] = {"bootdev", *strval};
    ipmi_chassis_main(self->interface, sizeof(argv)/sizeof(*argv), argv);
} V8_SET_END()

V8_EGET(NodeIpmi, GetUsers) {
    NodeIpmi *self = Unwrap(info.Holder());
    Handle<Array> list = Array::New();
    struct user_access_rsp access;

    int rc = ipmi_get_user_access(self->interface, IPMI_LAN_CHANNEL_E, 1, &access);

    int ndx = 0;
    for (int id = 1; id <= access.maximum_ids; id++) {
        char name[17]; // 16+NULL must be the max length in the spec?
        rc = ipmi_get_user_name(self->interface, id, name);
        if (rc) V8_STHROW(v8u::Err("Problem fetching username"));

        NodeIpmiUser *user = new NodeIpmiUser(self->interface, id);
        list->Set(ndx++, user->Wrapped());
    }
    V8_RET(list);
} V8_GET_END()
