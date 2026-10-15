<script setup>
import { state, t } from '../state';

const themes = [
  {
    value: 'light',
    zh: '浅色',
    en: 'Light',
    background: '#f5f6f8',
    panel: '#ffffff',
    accent: '#304fd0',
    line: '#dbe0e7'
  },
  {
    value: 'dark',
    zh: '深色',
    en: 'Dark',
    background: '#151920',
    panel: '#202630',
    accent: '#a9b9ff',
    line: '#424c5b'
  },
  {
    value: 'warm',
    zh: '暖色',
    en: 'Warm',
    background: '#f5f2ec',
    panel: '#fffdf8',
    accent: '#65532a',
    line: '#dcd5c6'
  },
  {
    value: 'midnight',
    zh: '深空',
    en: 'Midnight',
    background: '#101219',
    panel: '#191c27',
    accent: '#b5a7ff',
    line: '#41475c'
  },
  {
    value: 'paper',
    zh: '纸白',
    en: 'Paper',
    background: '#f4f5fa',
    panel: '#ffffff',
    accent: '#7057ca',
    line: '#dce0eb'
  },
  {
    value: 'forest',
    zh: '松林',
    en: 'Forest',
    background: '#101a18',
    panel: '#192923',
    accent: '#b9d49a',
    line: '#43594b'
  }
];

function previewStyle(theme) {
  return {
    '--preview-bg': theme.background,
    '--preview-panel': theme.panel,
    '--preview-accent': theme.accent,
    '--preview-line': theme.line
  };
}
</script>

<template>
  <header class="page-header">
    <div>
      <h1>{{ t('设置', 'Settings') }}</h1>
      <p>
        {{ t('调整显示方式，修改即时生效。', 'Changes take effect immediately.') }}
      </p>
    </div>
  </header>

  <div class="settings-stack">
    <!-- 风格切换 -->
    <section class="panel">
      <fieldset>
        <legend>{{ t('界面风格', 'Appearance') }}</legend>

        <div class="appearance-grid">
          <label
            v-for="theme in themes"
            :key="theme.value"
            class="appearance-option"
            :class="{ selected: state.theme === theme.value }"
            :style="previewStyle(theme)"
          >
            <div class="appearance-title">
              <input
                v-model="state.theme"
                type="radio"
                name="appearance"
                :value="theme.value"
              >

              <span>{{ t(theme.zh, theme.en) }}</span>

              <span
                v-if="state.theme === theme.value"
                class="selected-label"
              >
                {{ t('已选', 'Selected') }}
              </span>
            </div>

            <!-- 简化的界面预览 -->
            <div class="appearance-preview" aria-hidden="true">
              <div class="preview-sidebar">
                <span class="preview-logo"></span>
                <span class="preview-nav active"></span>
                <span class="preview-nav"></span>
                <span class="preview-nav"></span>
              </div>

              <div class="preview-content">
                <span class="preview-heading"></span>

                <div class="preview-columns">
                  <div class="preview-form">
                    <span></span>
                    <span></span>
                    <span class="preview-button"></span>
                  </div>

                  <div class="preview-result">
                    <span></span>
                    <strong>21</strong>
                  </div>
                </div>
              </div>
            </div>
          </label>
        </div>
      </fieldset>
    </section>

    <!-- 字号 -->
    <section class="panel">
      <fieldset>
        <legend>{{ t('文字大小', 'Text size') }}</legend>

        <div class="radio-options">
          <label>
            <input
              v-model="state.fontSize"
              type="radio"
              name="font-size"
              value="normal"
            >
            {{ t('标准（16 像素）', 'Standard (16 px)') }}
          </label>

          <label>
            <input
              v-model="state.fontSize"
              type="radio"
              name="font-size"
              value="large"
            >
            {{ t('较大（18 像素）', 'Large (18 px)') }}
          </label>
        </div>
      </fieldset>
    </section>

    <!-- 语言 -->
    <section class="panel">
      <fieldset>
        <legend>{{ t('语言', 'Language') }}</legend>

        <div class="radio-options">
          <label>
            <input
              v-model="state.lang"
              type="radio"
              name="language"
              value="zh"
            >
            简体中文
          </label>

          <label>
            <input
              v-model="state.lang"
              type="radio"
              name="language"
              value="en"
            >
            English
          </label>
        </div>
      </fieldset>
    </section>
  </div>
</template>

<style scoped>
/*
 * 新增三种主题。
 * 使用全局主题变量，使切换结果作用于所有页面。
 * 原有 light / dark / warm 继续使用 style.css 中的定义。
 */

/* 深空 */
:global(:root[data-theme="midnight"]) {
  color-scheme: dark;

  --bg: #101219;
  --panel: #191c27;
  --sidebar: #14161f;
  --field: #12151e;
  --text: #eef0f9;
  --muted: #aab1c6;
  --line: #41475c;

  --primary: #b5a7ff;
  --primary-hover: #c8bdff;
  --soft: #302842;

  --danger: #ffacba;
  --danger-bg: #3c2531;
  --success: #88dfba;

  --shadow: 0 8px 28px #00000018;
}

/* 纸白 */
:global(:root[data-theme="paper"]) {
  color-scheme: light;

  --bg: #f4f5fa;
  --panel: #ffffff;
  --sidebar: #ffffff;
  --field: #f8f9fc;
  --text: #242539;
  --muted: #62697e;
  --line: #dce0eb;

  --primary: #7057ca;
  --primary-hover: #5941b1;
  --soft: #eee9fb;

  --danger: #ad243b;
  --danger-bg: #fff1f2;
  --success: #176b49;

  --shadow: 0 5px 20px #24253906;
}

/* 松林 */
:global(:root[data-theme="forest"]) {
  color-scheme: dark;

  --bg: #101a18;
  --panel: #192923;
  --sidebar: #13201c;
  --field: #11201b;
  --text: #e5f0df;
  --muted: #adbfaf;
  --line: #43594b;

  --primary: #b9d49a;
  --primary-hover: #cde4b3;
  --soft: #2d4029;

  --danger: #ffb0ad;
  --danger-bg: #3b2927;
  --success: #9bdfb8;

  --shadow: 0 8px 28px #00000014;
}

/* 深色主题的浅色主按钮使用深色文字，保持对比度。 */
:global(:root[data-theme="midnight"] .button.primary) {
  color: #181329;
}

:global(:root[data-theme="forest"] .button.primary) {
  color: #152011;
}

/* 当前页面的主题选择卡片 */
.appearance-grid {
  display: grid;
  grid-template-columns: repeat(3, minmax(0, 1fr));
  gap: 16px;
}

.appearance-option {
  display: block;
  min-width: 0;
  padding: 15px;
  border: 2px solid var(--line);
  border-radius: 10px;
  background: var(--panel);
  cursor: pointer;
}

.appearance-option:hover {
  border-color: var(--primary);
}

.appearance-option.selected {
  border-color: var(--primary);
  background: var(--soft);
}

.appearance-option:focus-within {
  outline: 3px solid var(--primary);
  outline-offset: 3px;
}

.appearance-title {
  display: flex;
  align-items: center;
  gap: 8px;
  min-height: 28px;
  font-weight: 600;
}

.appearance-title input {
  flex-shrink: 0;
  margin: 0;
}

.selected-label {
  margin-left: auto;
  color: var(--primary);
  font-size: 0.75rem;
  font-weight: 500;
}

/* 卡片内预览 */
.appearance-preview {
  display: grid;
  grid-template-columns: 34px minmax(0, 1fr);
  height: 106px;
  margin-top: 14px;
  overflow: hidden;
  border: 1px solid var(--preview-line);
  border-radius: 6px;
  background: var(--preview-bg);
}

.preview-sidebar {
  padding: 12px 7px;
  border-right: 1px solid var(--preview-line);
  background: var(--preview-panel);
}

.preview-logo {
  display: block;
  width: 13px;
  height: 13px;
  margin-bottom: 13px;
  border-radius: 4px;
  background: var(--preview-accent);
}

.preview-nav {
  display: block;
  height: 5px;
  margin-top: 8px;
  border-radius: 3px;
  background: var(--preview-line);
}

.preview-nav.active {
  background: var(--preview-accent);
}

.preview-content {
  min-width: 0;
  padding: 13px 10px;
}

.preview-heading {
  display: block;
  width: 45%;
  height: 6px;
  margin-bottom: 13px;
  border-radius: 3px;
  background: var(--preview-accent);
}

.preview-columns {
  display: grid;
  grid-template-columns: 1.2fr 1fr;
  gap: 7px;
}

.preview-form,
.preview-result {
  height: 56px;
  padding: 8px;
  border: 1px solid var(--preview-line);
  border-radius: 4px;
  background: var(--preview-panel);
}

.preview-form > span {
  display: block;
  height: 7px;
  margin-bottom: 5px;
  border: 1px solid var(--preview-line);
  border-radius: 2px;
}

.preview-form > .preview-button {
  width: 55%;
  height: 9px;
  margin-top: 6px;
  margin-left: auto;
  border: 0;
  background: var(--preview-accent);
}

.preview-result > span {
  display: block;
  width: 60%;
  height: 4px;
  margin-bottom: 5px;
  border-radius: 2px;
  background: var(--preview-line);
}

.preview-result > strong {
  color: var(--preview-accent);
  font-size: 19px;
  font-weight: 600;
  line-height: 1.4;
}

@media (max-width: 1050px) {
  .appearance-grid {
    grid-template-columns: repeat(2, minmax(0, 1fr));
  }
}

@media (max-width: 480px) {
  .appearance-grid {
    grid-template-columns: 1fr;
  }

  .appearance-preview {
    height: 112px;
  }

  .appearance-option {
    padding: 16px;
  }
}
</style>