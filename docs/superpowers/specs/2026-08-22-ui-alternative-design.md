# CodeQA Warm Workbench UI Alternative — Design

**Date:** 2026-08-22  
**Branch:** `UI-Alternative`  
**Status:** Visual direction approved; written specification pending user review

## 1. Goal

Redesign the CodeQA desktop UI with the approved **Warm Workbench** visual
language. The application should feel like a calm, readable code-quality
workspace rather than a collection of dark utility panels, while preserving the
current Analyze, History, and Results workflows and the existing C++ facade.

The first implementation slice is the shared shell plus the findings-first
Results page. Analyze and History then adopt the same visual system without
changing their behavior or persistence contracts.

## 2. Product decisions

- Keep the existing three-page information architecture: Analyze, History, and
  Results.
- Keep all visible UI strings in English, matching the current desktop product.
- Keep Results read-only and findings-first.
- Keep analysis, report import, configuration, snapshots, and recent-path
  behavior in `AppController` and the existing core services.
- Use a light-first palette based on warm paper surfaces, charcoal text, stone
  borders, amber action accents, green success states, red errors, and muted
  blue rule links.
- Keep the UI self-contained in CodeQA. The QML-SnippetsExamples repository is
  a reference and pattern source, not a runtime or build dependency.
- Prefer responsive QML layouts and reusable components over page-specific
  pixel coordinates.

## 3. Approved visual direction

Warm Workbench uses a light paper-like canvas (`#f3eee6`) with near-white
surfaces (`#fffdf9`), dark charcoal content (`#342f2a`), stone borders
(`#ded5c8`), and amber primary actions (`#d59a3f`). The
sidebar is a deep muted green-charcoal (`#2f3835`) with a pale amber active
item (`#f0c982`).

The visual hierarchy is:

1. The top bar establishes product, project, branch, and user context.
2. The sidebar establishes the workspace and the current page.
3. The page heading explains the task and exposes the main action.
4. Summary cards give a short quality snapshot before dense data.
5. The list/table remains the primary report surface.
6. A dedicated detail card explains the selected finding without relying on
   low-contrast text or an overlay.

Effects are restrained. Warm Workbench does not use the current full-height
glass blur as a foundational treatment. Borders, surface contrast, spacing,
and small status accents provide depth; decorative gradients are optional and
must not reduce text contrast.

## 4. Scope and boundaries

### In scope

- A CodeQA theme/token singleton for colors, typography, spacing, radii, and
  common control dimensions.
- A persistent Workbench shell with top bar, sidebar navigation, project
  context, and page content area.
- Reusable surface, button, status, summary-card, filter, finding-row, and
  detail-card components.
- A Results page matching the approved HTML mockup:
  - quality-gate and finding summary cards;
  - severity, rule, file, and text filters;
  - priority findings list with selected-row state;
  - selected finding detail panel with recommendation, location, observed and
    limit values, and function metrics;
  - report export and run-analysis actions where the current controller
    permits them.
- Analyze and History restyling using the same shell and component vocabulary.
- Responsive behavior for the existing minimum desktop size and narrower
  windows.
- QML smoke-test updates and object names for the new navigational and Results
  surfaces.

### Out of scope

- Changes to analysis rules, report schema, configuration semantics, snapshot
  storage, or `AppController` business behavior.
- A source-code editor, inline suppression editor, or finding mutation flow.
- A runtime import of QML-SnippetsExamples or a copy of its entire style module.
- New analytics, network features, user accounts, or collaboration behavior.
- A separate web application. The HTML is a visual design artifact; QML is the
  product implementation.

## 5. Component architecture

The existing Qt-free core and C++ desktop adapter remain unchanged unless a
small presentation property is proven necessary. QML receives the existing
models and controller signals and owns only composition, styling, selection,
and visual state.

```text
ApplicationWindow (Main.qml)
├── WorkbenchHeader.qml
├── WorkbenchSidebar.qml
│   ├── NavigationItem.qml
│   └── ProjectContext.qml
└── StackLayout
    ├── AnalyzePage.qml
    ├── HistoryPage.qml
    └── ResultsPage.qml
        ├── SummaryCard.qml
        ├── FilterBar.qml
        ├── FindingTable.qml
        │   └── FindingRow.qml
        └── FindingDetails.qml
```

### Proposed QML files

- `src/desktop/qml/theme/CodeQaTheme.qml` — singleton for the Warm Workbench
  tokens and shared helper functions.
- `src/desktop/qml/controls/WorkbenchSurface.qml` — common bordered surface
  with optional elevated, selected, or tinted state.
- `src/desktop/qml/controls/WorkbenchButton.qml` — primary, secondary, and
  quiet button variants with disabled, hovered, pressed, and focused states.
- `src/desktop/qml/controls/StatusBadge.qml` — semantic success, error,
  warning, neutral, and not-evaluated states.
- `src/desktop/qml/controls/SummaryCard.qml` — label, value, trend, and status
  presentation for report summaries.
- `src/desktop/qml/controls/FilterBar.qml` — filter controls and search field
  framing; page-specific filter models remain in the page.
- `src/desktop/qml/controls/WorkbenchSidebar.qml` — workspace navigation and
  project context.
- `src/desktop/qml/controls/WorkbenchHeader.qml` — CodeQA identity, project
  context, branch/status context, and global actions.
- `src/desktop/qml/controls/FindingRow.qml` — one finding row with severity,
  rule, path, symbol, location, and selected state.
- `src/desktop/qml/controls/FindingDetails.qml` — selected finding detail
  presentation with recommendation, location, metrics, and read-only actions.
- `src/desktop/qml/pages/ResultsPage.qml` — page composition and controller
  bindings; it should not own the visual implementation of every row or card.

The exact split may remain smaller if a component would only be used once. The
shell, theme, summary card, finding row, and detail card are required
boundaries because they represent repeated visual rules or independent state.

## 6. Theme tokens

`CodeQaTheme.qml` should expose named values rather than scattering literals
through page files.

### Color roles

| Role | Initial value | Use |
| --- | --- | --- |
| `canvas` | `#f3eee6` | Main application background |
| `surface` | `#fffdf9` | Cards, lists, dialogs |
| `surfaceSubtle` | `#ebe4d9` | Filter bars and secondary regions |
| `ink` | `#342f2a` | Primary text |
| `mutedInk` | `#81776b` | Secondary text and metadata |
| `border` | `#ded5c8` | Surface and row separators |
| `sidebar` | `#2f3835` | Persistent navigation |
| `sidebarText` | `#dbe3dc` | Navigation text |
| `accent` | `#d59a3f` | Primary action and selection |
| `accentSoft` | `#f6e7c7` | Selected row and soft accent backgrounds |
| `success` | `#4f9276` | Passed quality gate and success badges |
| `error` | `#bd5e51` | Error severity |
| `warning` | `#b27b2d` | Warning severity |
| `link` | `#4f6585` | Rule IDs and navigable metadata |

The singleton should also expose spacing steps, standard corner radii,
sidebar width, content maximum width, row heights, and font sizes. Components
must consume role names so a later dark theme or high-contrast refinement does
not require editing every page.

## 7. Page behavior

### 7.1 Shell

`Main.qml` retains the `selectedPage` property and controller persistence. It
replaces the decorative glass backdrop and `PulseButton`-only navigation with
the Workbench shell. The sidebar remains visible on desktop widths. At narrow
widths it collapses to a compact navigation treatment without removing access
to Analyze, History, Results, Open report, or Clear recent paths.

The header displays the CodeQA mark, current project path/name, and a compact
context indicator. It does not introduce new project state; all values come
from existing controller properties or safe derived presentation values.

### 7.2 Results

Results remains read-only and keeps the current controller-facing model names
and filtering methods. The page composes:

- heading and primary actions;
- four summary cards for quality-gate status, matching findings, errors, and
  warnings;
- a filter bar wired to the existing severity, rule, and search properties;
- a list/table driven by `FindingListModel`;
- a detail panel driven by the current selected finding.

Selecting a row updates the detail panel. Filtering clears a selection that is
no longer present. When there is no report, no result, or no selected finding,
the page shows an intentional empty state in the same Warm Workbench surface
language rather than an empty white area.

The current export action remains available. Buttons that trigger existing
controller behavior remain disabled while analysis is running, exactly as they
are today.

### 7.3 Analyze

Analyze keeps the existing semantic configuration controls and progress states.
The redesign changes grouping and readability:

- project selection and analysis action become the page header;
- configuration status and errors use `StatusBadge` and an inline message;
- rules, header overrides, and quality gate become separate Workbench
  surfaces;
- thresholds remain editable with the same validation and controller calls;
- active analysis keeps the current phase and disables conflicting actions.

### 7.4 History

History keeps project selection, refresh, snapshot ordering, opening,
comparison, deletion confirmation, and inline error behavior. Snapshot rows
become Workbench surfaces with explicit timestamp, source project, status, and
actions. The delete action remains confirmation-gated and does not move into a
generic visual shortcut.

## 8. Reference-repository reuse

The implementation should borrow patterns from these reference files without
creating a cross-repository dependency:

- `mainui/home/HomePage.qml`, `MainMenu.qml`, and `Header.qml` for the
  persistent shell boundary and signal forwarding between navigation and
  content.
- `mainui/home/Dashboard.qml` for page-level lazy loading and a clear page map
  if the shell later grows beyond the current three pages.
- `examples/layouts/NestedDashboard.qml` for KPI card composition, nested
  `RowLayout`/`ColumnLayout`, and repeater-driven summary blocks.
- `examples/layouts/ResponsiveBreakpoints.qml` and
  `examples/layouts/CollapsibleSidebar.qml` for the narrow-window strategy.
- `styles/qmlsnippetsstyle/Button.qml` and
  `styles/qmlsnippetsstyle/buttons/PulseButton.qml` for control-template slots,
  state-driven backgrounds, and focus/hover behavior.
- `examples/frostglass/GlassCard.qml` as a reference for layered surfaces,
  though the approved direction intentionally uses much less blur.

Only the patterns that fit CodeQA's existing Qt module and behavior should be
adapted. Decorative effects from the reference repository are not a goal by
themselves.

## 9. CMake and resource integration

The QML module remains `CodeQA.Desktop`. New QML files must be registered in
`src/desktop/CMakeLists.txt` and receive stable resource aliases where the
existing module uses them. `CodeQaTheme.qml` must be marked with the Qt QML
singleton source-file property so `qt_add_qml_module` emits it as a singleton
in the generated module metadata.

The QML smoke-test resource list in `tests/CMakeLists.txt` must include every
new file needed to instantiate `Main.qml` offscreen. The test fixture should
continue to provide the same controller properties and models; UI-only theme
changes must not require a new C++ fixture protocol.

## 10. Verification strategy

- Run the existing Qt-free build and full CTest suite to ensure no core or CLI
  behavior changes.
- Build the Qt desktop target in Debug with warnings as errors.
- Extend `tests/desktop/qml/tst_DesktopShell.qml` to verify:
  - the three navigation items still exist and select the correct page;
  - the Warm Workbench Results heading and summary cards are present;
  - the finding list uses the controller model counts;
  - selecting a finding populates the detail panel;
  - filtering a selected finding clears the detail panel;
  - Results remains read-only and exposes no mutation control;
  - Analyze and History retain their existing object names and key actions.
- Run `codeqa_desktop_qml_smoke` with `QT_QPA_PLATFORM=offscreen`.
- Manually inspect the desktop at the existing minimum size and at a narrow
  window size to confirm that the sidebar, filters, findings table, and detail
  panel remain usable.
- Confirm that deployed QML resources launch through the existing
  `codeqa_desktop_deploy` flow.

## 11. Acceptance criteria

- The desktop shell visibly matches the approved Warm Workbench mockup: warm
  canvas, dark green-charcoal sidebar, amber active navigation/action, clear
  borders, and readable dark text.
- Results opens as a calm findings-first workspace with summary context,
  filters, a readable findings list, and a dedicated selected-finding detail
  card.
- Analyze and History use the same theme and component vocabulary while
  preserving their current behaviors and controller calls.
- No C++ application-service or report-model behavior changes are required for
  the visual migration.
- Existing Qt-free and Qt/QML test suites pass.
- The implementation does not require the external QML-SnippetsExamples path
  to build or run.
