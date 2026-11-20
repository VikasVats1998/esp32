// Lets an action through at most once every `ms` milliseconds (used for live slider updates).
export function makeThrottle(ms: number) {
  let last = 0;
  return (force = false) => {
    const now = Date.now();
    if (force || now - last >= ms) {
      last = now;
      return true;
    }
    return false;
  };
}
