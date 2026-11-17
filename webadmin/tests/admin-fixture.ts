import {ADMIN_ACTIONS} from "../src/lib/admin";
export const administration={maintenance:true,actions:ADMIN_ACTIONS.map((key,i)=>({key,available:i<6,local_owner_only:i>=6})),statistics:{attempts:"9",succeeded:"5",refused:"2",unavailable:"1",outcome_unknown:"1"}};
