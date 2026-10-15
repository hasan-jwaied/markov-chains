<script lang="ts" generics="T extends string | number">
  let {
    options,
    value = $bindable(),
    label,
    onchange,
  }: {
    options: Array<{ value: T; label: string; title?: string }>;
    value: T;
    label?: string;
    onchange?: (v: T) => void;
  } = $props();
</script>

<div class="wrap">
  {#if label}<span class="label">{label}</span>{/if}
  <div class="seg" role="radiogroup" aria-label={label}>
    {#each options as opt (opt.value)}
      <button
        role="radio"
        aria-checked={value === opt.value}
        title={opt.title}
        class:on={value === opt.value}
        onclick={() => {
          value = opt.value;
          onchange?.(opt.value);
        }}>{opt.label}</button
      >
    {/each}
  </div>
</div>

<style>
  .wrap {
    display: flex;
    flex-direction: column;
    gap: 6px;
  }
  .label {
    font: 500 13px var(--font-sans);
    color: var(--muted);
  }
  .seg {
    display: inline-flex;
    flex-wrap: wrap;
    width: max-content;
    max-width: 100%;
    padding: 2px;
    gap: 2px;
    border-radius: 999px;
    background: var(--fill);
  }
  button {
    display: inline-flex;
    align-items: center;
    justify-content: center;
    min-height: 30px;
    padding: 0 14px;
    border: 0;
    border-radius: 999px;
    background: transparent;
    color: var(--ink);
    font: 500 13px var(--font-sans);
    white-space: nowrap;
    transition: background-color 0.15s;
  }
  button:not(.on):hover {
    background: var(--hover);
  }
  button.on {
    background: var(--thumb);
    font-weight: 600;
    box-shadow:
      0 3px 8px rgb(0 0 0 / 0.12),
      0 3px 1px rgb(0 0 0 / 0.04);
  }
</style>
