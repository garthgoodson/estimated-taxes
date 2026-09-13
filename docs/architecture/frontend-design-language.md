# Frontend Design Language

## Purpose

Provide a small, reusable visual and interaction baseline for the tax application without creating a large custom design system.

The structural baseline is the Nuxt UI Dashboard. The visual direction is a restrained dark financial dashboard: clear hierarchy, compact supporting information, and emphasis on trustworthy numeric presentation.

## Design principles

1. **Data before decoration:** amounts, labels, and status must be easier to notice than ornamental styling.
2. **Dark but readable:** use charcoal and slate surfaces rather than pure black, with accessible text contrast.
3. **Limited color:** reserve accent colors for actions, status, jurisdiction identity, and chart series.
4. **Honest precision:** actual, projected, paid, and recommended values must always be distinguishable through a nearby label or an unambiguous enclosing heading.
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
- Keep labels close to values and include jurisdiction, time period, and actual/projected meaning where ambiguity is possible. An enclosing subsection may supply shared context; do not repeat it in every child label.
- Organize summaries by financial role. Use subsection headings for shared concepts, concise child labels for the differing dimension (for example, Federal and California), and low-contrast dividers between unlike groups.
- Use a two-column metric grid for comparable Federal and California values. A group with four directly comparable investment metrics may use four columns on wide screens, then collapse to two and one columns.
- Reserve a large amount treatment for one primary result per card. Supporting values use a consistent compact label/value treatment.
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

Use Nuxt UI button `color` and `variant` props as the semantic button classifier. Layout classes may position a button but must not define its meaning. Destructive removal actions use the error outline treatment and sit on their own form row at natural width.

## Modal pattern

Use Nuxt UI `UModal` directly. Every application modal has a clear action title, an optional one-sentence purpose in the standard header, a scrollable body when its task can exceed the viewport, and a footer with a neutral outline **Cancel** action on the left and the validated primary action on the right. Place validation and cautions beside the affected body content. Destructive modal actions use the error color and state the irreversible consequence explicitly.

Do not create a modal wrapper until more than one substantial modal demonstrates a shared application behavior beyond this documented composition. Native browser confirmations remain outside this pattern until they are deliberately migrated.

## Summary and form patterns

- A multi-jurisdiction recommendation has one shared section title. Each Federal or California card contains the jurisdiction heading, a muted outcome line, one aligned current-amount row, and a two-column supporting-details grid. A divider separates the current amount from supporting details.
- Reuse meaningful group headings rather than repeating their wording in metric labels. For example, under **Tax withholding**, child labels are **Federal** and **California**, not repeated withholding phrases.
- Empty states live inside their enclosing card without adding a second visible ring or border.
- Form section headings establish shared context. Paystub labels therefore omit repeated **Current pay period** and **Year to date** wording, use title case, and preserve recognized acronyms such as **SDI**.
- Spouse-facing headings use the saved household label, with the stable spouse key retained only for data mapping.
- A paystub may use a compact, backend-derived projection-horizon control with year-end, after-this-paystub, and selected-date choices. It collects the end-date fact without calculating periods, wages, withholding, or tax in the browser.
- A gross-pay calculator may normalize user-entered payroll amounts into existing paystub fields entirely in the browser. It shows its arithmetic, requires explicit Apply, retains manual editability, and never stores its deduction breakdown. This narrowly scoped input convenience must not calculate tax liability, encode tax rules, or become a precedent for frontend tax behavior.
- Quarter investment activity uses editable aggregates with an **Add** modal for signed adjustments. The modal updates one displayed total at a time; it does not imply or store individual investment transactions.

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
