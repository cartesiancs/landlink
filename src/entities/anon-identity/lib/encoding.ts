// Compact, human-readable form of the account id for UI ("acct_ab12…cd34").
// Generic byte codecs live in @/shared/lib; this file holds only the
// account-specific display helper.
export function shortAccountId(accountId: string): string {
  if (accountId.length <= 12) return `acct_${accountId}`;
  return `acct_${accountId.slice(0, 6)}…${accountId.slice(-4)}`;
}
