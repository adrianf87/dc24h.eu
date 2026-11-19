import type { PermissionKey } from "./roles.ts";

export const MODERATION_OPERATIONS = ["kick", "mute", "unmute", "ban_account", "ban_cid", "ban_address", "revoke_ban"] as const;
export const MODERATION_SUBJECTS = ["session", "account", "cid", "address", "ban_record"] as const;
export const MODERATION_TARGETS = ["lower_role", "lower_account_or_unmapped", "permission_only", "creator_or_lower"] as const;
export type ModerationOperation = typeof MODERATION_OPERATIONS[number];
export type ModerationSubject = typeof MODERATION_SUBJECTS[number];
export type ModerationTargetRule = typeof MODERATION_TARGETS[number];
export interface ModerationRule {
  operation: ModerationOperation;
  permission: PermissionKey;
  subject: ModerationSubject;
  target: ModerationTargetRule;
  guest_target: boolean;
  forbid_self: boolean;
  vip_delegable: boolean;
  requires_blocks: boolean;
}
export const MODERATION_RULES: readonly ModerationRule[] = [
  { operation: "kick", permission: "users.kick", subject: "session", target: "lower_role", guest_target: true, forbid_self: true, vip_delegable: true, requires_blocks: false },
  { operation: "mute", permission: "users.mute", subject: "session", target: "lower_role", guest_target: true, forbid_self: true, vip_delegable: true, requires_blocks: false },
  { operation: "unmute", permission: "users.mute", subject: "session", target: "lower_role", guest_target: true, forbid_self: true, vip_delegable: true, requires_blocks: false },
  { operation: "ban_account", permission: "users.ban", subject: "account", target: "lower_account_or_unmapped", guest_target: false, forbid_self: true, vip_delegable: false, requires_blocks: true },
  { operation: "ban_cid", permission: "users.ban", subject: "cid", target: "lower_account_or_unmapped", guest_target: false, forbid_self: true, vip_delegable: false, requires_blocks: true },
  { operation: "ban_address", permission: "users.ban", subject: "address", target: "permission_only", guest_target: false, forbid_self: false, vip_delegable: false, requires_blocks: true },
  { operation: "revoke_ban", permission: "users.ban", subject: "ban_record", target: "creator_or_lower", guest_target: false, forbid_self: false, vip_delegable: false, requires_blocks: true },
];

function record(value: unknown): Record<string, unknown> {
  if (!value || typeof value !== "object" || Array.isArray(value)) throw new Error("Invalid moderation rules");
  return value as Record<string, unknown>;
}
export function parseModeration(value: unknown): ModerationRule[] {
  if (!Array.isArray(value) || value.length !== MODERATION_RULES.length) throw new Error("Invalid moderation rules");
  return MODERATION_RULES.map((expected, index) => {
    const row = record(value[index]);
    const keys = ["operation", "permission", "subject", "target", "guest_target", "forbid_self", "vip_delegable", "requires_blocks"] as const;
    for (const key of keys) {
      if (row[key] !== expected[key]) throw new Error("Invalid moderation rules");
    }
    return { ...expected };
  });
}
