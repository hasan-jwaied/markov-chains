<script lang="ts">
  let { label, value, note }: { label: string; value: string; note?: string } = $props();
</script>

<div class="tile glass">
  <div class="label">{label}</div>
  <div class="value">{value}</div>
  {#if note}<div class="note">{note}</div>{/if}
</div>

<style>
  .tile {
    padding: 20px 22px;
    border-radius: 18px;
    min-width: 0;
  }
  .label {
    order: 2;
    margin-top: 6px;
    font-size: 14px;
    color: var(--muted);
  }
  .value {
    font-size: clamp(24px, 2.6vw, 32px);
    font-weight: 600;
    color: var(--ink-strong);
    letter-spacing: -0.02em;
    line-height: 1.1;
    overflow-wrap: anywhere;
  }
  .note {
    order: 3;
    font-size: 12.5px;
    color: var(--muted);
    margin-top: 2px;
  }
  .tile {
    display: flex;
    flex-direction: column;
  }
</style>
