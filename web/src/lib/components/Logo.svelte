<script lang="ts">
  /** The project mark: Pixel Lab's rounded square, in this project's indigo. */
  let { size = 26 }: { size?: number } = $props();
</script>

<svg width={size} height={size} viewBox="0 0 32 32" aria-hidden="true" focusable="false">
  <rect x="1" y="1" width="30" height="30" rx="8" fill="var(--accent)" />
  <g fill="none" stroke="#fff" stroke-width="1.7" stroke-linecap="round" opacity="0.72">
    <path d="M13.4 11.8 10.6 17.6" />
    <path d="M18.6 11.8l2.8 5.8" />
    <path d="M13.2 22.4h5.6" />
  </g>
  <g fill="#fff">
    <circle cx="16" cy="9.4" r="3.1" />
    <circle cx="9.3" cy="21.6" r="3.1" opacity="0.85" />
    <circle cx="22.7" cy="21.6" r="3.1" opacity="0.85" />
  </g>
</svg>
