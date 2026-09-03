# Frontend Design Language

## Purpose

Provide a small, reusable visual and interaction baseline for the tax application without creating a large custom design system.

The structural baseline is the Nuxt UI Dashboard. The visual direction is a restrained dark financial dashboard: clear hierarchy, compact supporting information, and emphasis on trustworthy numeric presentation.

## Design principles

1. **Data before decoration:** amounts, labels, and status must be easier to notice than ornamental styling.
2. **Dark but readable:** use charcoal and slate surfaces rather than pure black, with accessible text contrast.
3. **Limited color:** reserve accent colors for actions, status, jurisdiction identity, and chart series.
4. **Honest precision:** actual, projected, paid, and recommended values must always be distinguishable.
5. **Consistent density:** forms and summaries should be compact enough for financial review without feeling crowded.
6. **Reuse proven structure:** start with Nuxt UI components and dashboard patterns; add application components only when the same composition repeats.

## Visual foundation

The initial theme uses semantic roles rather than page-specific colors:

| Role | Direction |
| --- | --- |
| Application background | Deep charcoal/slate |
| Navigation background | Slightly darker than the application background |
| Card and panel surface | Slightly lighter than the application background |
| Primary text | Near-white, not pure white |
| Secondary text | Muted cool gray |
| Border and divider | Low-contrast cool gray |
| Primary action | Clear blue |
| Positive/paid | Green |
| Warning/due | Amber |
| Error/overdue | Red |
| Projected value | Violet or a dashed treatment paired with a label |

Exact values should be defined through Nuxt UI semantic theme tokens and adjusted together. Pages must not introduce arbitrary colors.

Typography, spacing, border radius, focus rings, and shadows begin with Nuxt UI defaults. Change a default only when a representative application screen demonstrates a concrete need.

## Layout language

- Use the Nuxt UI Dashboard shell for navigation and page structure.
- Prefer a readable central content width over filling a large display edge to edge.
- Use cards to group related information, not to decorate every number.
- Give primary tax recommendations the strongest visual position on the Quarter page.
- Use large numeric values sparingly for headline results.
- Keep labels close to values and include jurisdiction, time period, and actual/projected meaning where ambiguity is possible.
- Use responsive stacking; do not create a separate mobile information architecture.

## Component policy

Use Nuxt UI components directly for general controls such as buttons, inputs, selects, dialogs, alerts, tables, and navigation.

Create a shared application component only when it provides one of these:

- Repeated financial formatting or behavior
- A repeated composition used on more than one page
- Consistent actual/projected or federal/California presentation
- Domain validation and help text that belongs together

Likely shared application components include money input/display, metric summary, jurisdiction summary, warning list, quarter status, and calculation breakdown. Tax-specific components stay in this application.

Avoid wrappers that merely rename one Nuxt UI component without adding shared behavior or meaning.

## Chart language

Charts are outside the MVP, but any later ECharts work follows these rules:

- Never use smoothed line interpolation.
- Show a visible marker for every recorded observation.
- Connect observations with thin straight segments.
- Represent missing data as missing, never as zero.
- Do not connect across missing observations unless the chart explicitly explains that choice.
- Distinguish actual and projected values with labels plus line style or marker shape; color alone is insufficient.
- Use subtle grid lines and minimal axis decoration.
- Use precise currency and percentage tooltips.
- Avoid gradients, 3D effects, decorative animation, and implied precision.
- Do not label every point when a tooltip is clearer, except on sparse charts where labels improve reading.

The reusable ECharts theme should draw its colors and typography from the same semantic theme source as the Nuxt interface. Chart-specific data mapping remains in the application.

## Reuse boundary

The reusable layer may eventually contain:

- Semantic theme tokens
- Nuxt UI theme configuration
- Shared financial display primitives
- Common dashboard layout patterns
- ECharts theme and formatting helpers
- Loading, empty, warning, and error presentation

It must not contain tax terminology, tax calculations, quarter-entry behavior, federal/California rules, or application API models.

Keep the reusable material inside this project initially. Extract it into a Nuxt Layer or package only after another project needs it; demonstrated reuse should determine the public API.

## Initial implementation boundary

For the first frontend implementation:

- Adopt the Nuxt UI Dashboard structure and dark mode.
- Configure one restrained semantic theme.
- Build only components required by Home, Quarter, History, and Settings.
- Do not implement charts.
- Validate the language against the Home and Quarter pages before expanding it.

## Reference

- [Nuxt UI Dashboard templates](https://ui.nuxt.com/templates)
- [Nuxt UI design-system theming](https://ui.nuxt.com/docs/getting-started/theme/design-system)
- [Nuxt UI Figma kit](https://ui.nuxt.com/figma)
- [ECharts line-chart guidance](https://echarts.apache.org/handbook/en/how-to/chart-types/line/basic-line/)
