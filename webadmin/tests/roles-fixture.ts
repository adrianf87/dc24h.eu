import {ROLE_KEYS,PERMISSION_KEYS} from "../src/lib/roles";
import type {PermissionKey,RolesSnapshot} from "../src/lib/roles";
export function rolePolicy(vip:PermissionKey[]=[]):RolesSnapshot {
 const basic:PermissionKey[]=["hub.use","chat.public","chat.private","search.queries","search.results","transfers.initiate","transfers.receive"];
 return {vip_permissions:vip,
  catalog:ROLE_KEYS.map((key,rank)=>({key,rank,permissions:PERMISSION_KEYS.filter(p=>basic.includes(p)||(rank>=1&&p==="accounts.password.change")||rank===6||
   (rank===2&&vip.includes(p))||(rank>=3&&["users.kick","users.mute","audit.read"].includes(p))||
   (rank>=4&&["users.ban","blocks.manage"].includes(p))||(rank>=5&&["accounts.manage","roles.manage","configuration.manage"].includes(p)))})),
  operations:PERMISSION_KEYS.map(key=>({key,implemented:!["configuration.manage","hub.manage"].includes(key)}))};
}
