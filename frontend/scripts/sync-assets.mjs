import { access, mkdir, copyFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';
const source = fileURLToPath(new URL('../../assets/MINAX.png', import.meta.url));
const target = fileURLToPath(new URL('../public/assets/', import.meta.url));
try { await access(source); await mkdir(target,{recursive:true}); await copyFile(source,target+'MINAX.png'); } catch { console.warn('MINAX.png not found at project/assets; keeping bundled icon if present.'); }
