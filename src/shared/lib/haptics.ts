export function hapticTick(): void {
  const isiOS = /iPhone|iPad|iPod/.test(navigator.userAgent);
  const supportsVibrate = typeof navigator.vibrate === "function";

  if (isiOS) {
    const input = document.createElement("input");
    input.type = "checkbox";
    input.setAttribute("switch", "");
    input.style.position = "absolute";
    input.style.opacity = "0";
    input.style.pointerEvents = "none";

    const id = "haptic-switch-" + Math.random().toString(36).slice(2);
    input.id = id;

    const label = document.createElement("label");
    label.htmlFor = id;

    document.body.appendChild(input);
    document.body.appendChild(label);

    label.click();
    label.click();

    input.remove();
    label.remove();
    return;
  }

  if (supportsVibrate) {
    navigator.vibrate(10);
  }
}
