import js from '@eslint/js'
import globals from 'globals'
import reactHooks from 'eslint-plugin-react-hooks'
import reactRefresh from 'eslint-plugin-react-refresh'
import tseslint from 'typescript-eslint'
import { defineConfig, globalIgnores } from 'eslint/config'

// Feature-Sliced Design layers — higher layers may import from lower layers only.
// Order: app > pages > widgets > features > entities > shared
const LAYERS = ['app', 'pages', 'widgets', 'features', 'entities', 'shared']

// Build a layer-boundary override: files in `layer` may not import from
// layers strictly higher than it (the ones before it in LAYERS).
function forbidHigherLayers(layer) {
  const higher = LAYERS.slice(0, LAYERS.indexOf(layer))
  if (higher.length === 0) return null
  return {
    files: [`src/${layer}/**/*.{ts,tsx}`],
    rules: {
      'no-restricted-imports': [
        'error',
        {
          patterns: higher.map((upper) => ({
            group: [`@/${upper}/*`, `@/${upper}`],
            message: `Layer boundary: src/${layer} cannot import from src/${upper} (higher layer).`,
          })),
        },
      ],
    },
  }
}

// Disallow reaching into another slice's internals — every slice must be
// consumed through its public API (index.ts at the slice root).
const sliceEncapsulation = {
  files: ['src/**/*.{ts,tsx}'],
  rules: {
    'no-restricted-imports': [
      'error',
      {
        patterns: [
          {
            group: [
              '@/pages/*/*',
              '@/widgets/*/*',
              '@/features/*/*',
              '@/entities/*/*',
            ],
            message:
              'Import slices through their public API (e.g. `@/features/auth`), not internal segments.',
          },
          {
            // Allow `../*` (staying inside the same slice, e.g. ui → assets),
            // but forbid `../../` or deeper — those always cross a slice/layer.
            group: ['../../*', '../../../*', '../../../../*'],
            message:
              'Use the `@/` alias for cross-slice/cross-layer imports. Only same-slice relative imports (./ and ../) are allowed.',
          },
        ],
      },
    ],
  },
}

export default defineConfig([
  globalIgnores(['dist', 'android', 'ios']),
  {
    files: ['**/*.{ts,tsx}'],
    extends: [
      js.configs.recommended,
      tseslint.configs.recommendedTypeChecked,
      tseslint.configs.stylisticTypeChecked,
      reactHooks.configs.flat.recommended,
      reactRefresh.configs.vite,
    ],
    languageOptions: {
      ecmaVersion: 2023,
      globals: globals.browser,
      parserOptions: {
        projectService: true,
        tsconfigRootDir: import.meta.dirname,
      },
    },
    rules: {
      '@typescript-eslint/consistent-type-imports': [
        'error',
        { prefer: 'type-imports', fixStyle: 'inline-type-imports' },
      ],
      '@typescript-eslint/no-unused-vars': [
        'error',
        { argsIgnorePattern: '^_', varsIgnorePattern: '^_' },
      ],
      '@typescript-eslint/no-floating-promises': 'error',
      '@typescript-eslint/no-misused-promises': 'error',
      '@typescript-eslint/switch-exhaustiveness-check': 'error',
      '@typescript-eslint/consistent-type-definitions': 'off',
      'eqeqeq': ['error', 'always'],
      'no-console': ['warn', { allow: ['warn', 'error'] }],
    },
  },
  sliceEncapsulation,
  ...LAYERS.map(forbidHigherLayers).filter(Boolean),
  // shadcn/ui primitives in shared/ui are template code that commonly
  // co-exports hooks/constants alongside components — relax Fast Refresh
  // warnings there. Business code must still keep files component-only.
  {
    files: ['src/shared/ui/**/*.{ts,tsx}'],
    rules: {
      'react-refresh/only-export-components': 'off',
    },
  },
])
