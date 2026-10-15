<script lang="ts">
  let {
    label,
    value = $bindable(),
    min,
    max,
    step = 1,
    format = (v: number) => String(v),
    width = 160,
    oninput,
  }: {
    label: string;
    value: number;
    min: number;
    max: number;
    step?: number;
    format?: (v: number) => string;
    width?: number;
    oninput?: (v: number) => void;
  } = $props();

  const id = `s-${Math.random().toString(36).slice(2, 9)}`;
  const pct = $derived(((value - min) / (max - min)) * 100);
</script>

<div class="slider">
  <label for={id}>{label} <output class="num" for={id}>{format(value)}</output></label>
  <input
    {id}
    type="range"
    {min}
    {max}
    {step}
    bind:value
    style:width="{width}px"
    style:--pct="{pct}%"
    oninput={() => oninput?.(value)}
  />
</div>

<style>
  .slider {
    display: flex;
    flex-direction: column;
    gap: 6px;
    font: 13px var(--font-sans);
  }
  label {
    color: var(--muted);
    font-weight: 500;
    display: flex;
    justify-content: space-between;
    gap: 12px;
  }
  output {
    color: var(--ink);
    font-weight: 600;
  }
  input {
    appearance: none;
    -webkit-appearance: none;
    height: 24px;
    background: transparent;
    cursor: pointer;
    margin: 0;
    max-width: 100%;
  }
  input::-webkit-slider-runnable-track {
    height: 4px;
    border-radius: 2px;
    background: linear-gradient(to right, var(--accent) var(--pct), var(--fill-hover) var(--pct));
  }
  input::-moz-range-track {
    height: 4px;
    border-radius: 2px;
    background: linear-gradient(to right, var(--accent) var(--pct), var(--fill-hover) var(--pct));
  }
  input::-webkit-slider-thumb {
    -webkit-appearance: none;
    width: 22px;
    height: 22px;
    margin-top: -9px;
    border-radius: 50%;
    background: #fff;
    border: 0;
    box-shadow: 0 0.5px 4px rgb(0 0 0 / 0.12), 0 6px 13px rgb(0 0 0 / 0.12), 0 0 0 0.5px rgb(0 0 0 / 0.04);
  }
  input::-moz-range-thumb {
    width: 22px;
    height: 22px;
    border-radius: 50%;
    background: #fff;
    border: 0;
    box-shadow: 0 0.5px 4px rgb(0 0 0 / 0.12), 0 6px 13px rgb(0 0 0 / 0.12);
  }
</style>
