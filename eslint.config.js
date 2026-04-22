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
            group: ['../*', '../../*', '../../../*'],
            message:
              'Use the `@/` alias instead of relative parent imports to keep layer boundaries explicit.',
          },
        ],
      },
    ],
  },
}

export default defineConfig([
  globalIgnores(['dist']),
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
])
