/*
  c64.js - tiny framework-free Commodore 64 module for the browser.

  Emulator core: floooh/chips c64.h (zlib license, Copyright (c) 2018 Andre
  Weissflog), compiled unmodified to c64.wasm with clang. See LICENSE-chips.txt.
  Firmware: MEGA65 Open ROMs (LGPL-3.0-or-later), loaded at runtime from roms/.
  No original Commodore ROMs are used or contained in any of these files.

  Usage:
    const c = await C64.create({ base: 'c64/' });
    document.body.appendChild(c.canvas);
    c.start();
    c.attachKeyboard(window);              // optional: forward real key events
    await c.typeText('10 PRINT "HOLA"\nRUN\n');
    await c.loadPrg(bytes); await c.run();
    c.muted = false;                       // call from a user gesture
*/
(function (global) {
  'use strict';

  const SAMPLE_RATE = 44100;
  const PAL_FRAME_US = 19950;   // 312 lines * 63 cycles / 985248 Hz
  // typeText key timing (emulated time); the Open ROMs keyboard scan needs
  // ~2 PAL frames of key-up between two presses of the same key
  const TYPE_DOWN_US = 40000, TYPE_UP_US = 40000;
  const CROPS = {
    full:   [0, 0, 392, 272],   // everything the core renders
    border: [0, 0, 384, 272],   // 32px side borders, 36px top/bottom
    screen: [32, 36, 320, 200], // only the 40x25 text/bitmap area
  };

  // C64 key codes understood by the chips keyboard matrix
  const K = {
    SPACE: 0x20, CSRLEFT: 0x08, CSRRIGHT: 0x09, CSRDOWN: 0x0A, CSRUP: 0x0B,
    DEL: 0x01, INST: 0x10, HOME: 0x0C, CLR: 0x02, RETURN: 0x0D, CTRL: 0x0E,
    CBM: 0x0F, RESTORE: 0xFF, STOP: 0x03, RUN: 0x07, LEFTARROW: 0x04,
    F1: 0xF1, F2: 0xF2, F3: 0xF3, F4: 0xF4, F5: 0xF5, F6: 0xF6, F7: 0xF7, F8: 0xF8,
  };
  // KeyboardEvent.code -> C64 key code (non-printable keys)
  const CODE_MAP = {
    Enter: K.RETURN, NumpadEnter: K.RETURN, Backspace: K.DEL, Delete: K.DEL,
    Insert: K.INST, Home: K.HOME, Escape: K.STOP, Tab: K.CTRL,
    ArrowLeft: K.CSRLEFT, ArrowRight: K.CSRRIGHT, ArrowUp: K.CSRUP, ArrowDown: K.CSRDOWN,
    F1: K.F1, F2: K.F2, F3: K.F3, F4: K.F4, F5: K.F5, F6: K.F6, F7: K.F7, F8: K.F8,
    PageUp: K.RESTORE, End: K.CLR, Space: K.SPACE, AltLeft: K.CBM, ControlLeft: K.CTRL,
    Backquote: K.LEFTARROW,
  };
  // printable characters registered in the chips C64 key map
  const PRINTABLE = new Set(
    '3WA4ZSE5RD6CFTX7YG8BHUV9IJ0MKON+PL-.:@,~*;=/^12Q#wa$zse%rd&cftx\'yg(bhuv)ijmkon pl>[<]?!"q'
  );
  const JOY = { UP: 1, DOWN: 2, LEFT: 4, RIGHT: 8, FIRE: 16 };

  function charToC64(ch, swapCase) {
    if (ch === '\n' || ch === '\r') return K.RETURN;
    if (ch === ' ') return K.SPACE;
    if (/^[a-zA-Z]$/.test(ch)) {
      // chips: upper-case = unshifted letter, lower-case = shift+letter
      if (!swapCase) return ch.toUpperCase().charCodeAt(0);
      return (ch === ch.toLowerCase() ? ch.toUpperCase() : ch.toLowerCase()).charCodeAt(0);
    }
    if (ch === '£') return '~'.charCodeAt(0);
    return PRINTABLE.has(ch) ? ch.charCodeAt(0) : 0;
  }

  async function fetchBytes(src) {
    if (src instanceof Uint8Array) return src;
    if (src instanceof ArrayBuffer) return new Uint8Array(src);
    const r = await fetch(src);
    if (!r.ok) throw new Error('c64: cannot load ' + src + ' (' + r.status + ')');
    return new Uint8Array(await r.arrayBuffer());
  }

  const WORKLET_SRC = `
    class C64Out extends AudioWorkletProcessor {
      constructor() {
        super(); this.q = []; this.cur = null; this.pos = 0; this.len = 0;
        this.port.onmessage = (e) => {
          this.q.push(e.data); this.len += e.data.length;
          while (this.len > ${SAMPLE_RATE / 4} && this.q.length > 1) { this.len -= this.q.shift().length; }
        };
      }
      process(inputs, outputs) {
        const out = outputs[0][0];
        for (let i = 0; i < out.length; i++) {
          if (!this.cur || this.pos >= this.cur.length) {
            this.cur = this.q.shift() || null; this.pos = 0;
            if (!this.cur) { out.fill(0, i); return true; }
            this.len -= this.cur.length;
          }
          out[i] = this.cur[this.pos++];
        }
        for (let c = 1; c < outputs[0].length; c++) outputs[0][c].set(out);
        return true;
      }
    }
    registerProcessor('c64-out', C64Out);`;

  class C64 {
    static async create(opts = {}) {
      const base = opts.base == null ? '' : (opts.base.endsWith('/') || opts.base === '' ? opts.base : opts.base + '/');
      const roms = Object.assign({
        kernal: base + 'roms/kernal_generic.rom',
        basic: base + 'roms/basic_generic.rom',
        chars: base + 'roms/chargen_openroms.rom',
      }, opts.roms || {});
      const wasmSrc = opts.wasm || base + 'c64.wasm';
      const [wasmBytes, kernal, basic, chars] = await Promise.all(
        [wasmSrc, roms.kernal, roms.basic, roms.chars].map(fetchBytes));
      if (kernal.length !== 8192 || basic.length !== 8192 || chars.length !== 4096) {
        throw new Error('c64: ROM sizes must be 8192/8192/4096 bytes');
      }
      const { instance } = await WebAssembly.instantiate(wasmBytes, {});
      return new C64(instance.exports, { kernal, basic, chars }, opts);
    }

    constructor(wasm, roms, opts) {
      this.w = wasm;
      this._roms = roms;
      this._crop = Array.isArray(opts.crop) ? opts.crop : (CROPS[opts.crop || 'border'] || CROPS.border);
      const [, , cw, ch] = this._crop;
      this.canvas = opts.canvas || document.createElement('canvas');
      this.canvas.width = cw; this.canvas.height = ch;
      this.canvas.style.imageRendering = 'pixelated';
      this._ctx = this.canvas.getContext('2d');
      this._img = this._ctx.createImageData(cw, ch);
      this._running = false; this._raf = 0; this._last = 0;
      this._typeQueue = []; this._typeState = 0; this._typeT = 0; this._typeWaiters = [];
      this._held = new Map();          // KeyboardEvent.code -> c64 key
      this._joy = [0, 0];
      this._muted = true; this._volume = opts.volume == null ? 0.5 : opts.volume;
      this._audio = null; this._audioRead = 0;
      this._readyWaiters = []; this._isReady = false; this._emuUs = 0;
      this._kbdTarget = null;
      this.onframe = null;             // optional callback after each rendered frame
      this._boot();
      this._loop = this._loop.bind(this);
      this._onKeyDown = (e) => { if (this.key(true, e.code, e.key)) e.preventDefault(); };
      this._onKeyUp = (e) => { if (this.key(false, e.code, e.key)) e.preventDefault(); };
      this._render();
    }

    _boot() {
      const m = new Uint8Array(this.w.memory.buffer);
      m.set(this._roms.kernal, this.w.rom_ptr(0));
      m.set(this._roms.basic, this.w.rom_ptr(1));
      m.set(this._roms.chars, this.w.rom_ptr(2));
      this.w.init(SAMPLE_RATE);
      this.w.joystick(this._joy[0], this._joy[1]);
      this._audioRead = this.w.audio_written();
      this._isReady = false; this._emuUs = 0;
    }

    /* ---- run control ---- */
    get running() { return this._running; }
    start() {
      if (this._running) return;
      this._running = true; this._last = 0;
      this._raf = requestAnimationFrame(this._loop);
    }
    stop() {
      this._running = false;
      if (this._raf) cancelAnimationFrame(this._raf);
      this._raf = 0;
    }
    reset() {             // hard reset (re-initializes the machine)
      this._typeQueue.length = 0; this._typeState = 0;
      this._boot(); this._flushTypeWaiters();
    }
    /* advance emulation by `us` microseconds (also usable while stopped) */
    step(us = PAL_FRAME_US) {
      this._typeTick(us);
      this.w.exec(us | 0);
      this._emuUs += us;
      this._pumpAudio();
      this._checkReady();
      this._render();
    }
    _loop(t) {
      if (!this._running) return;
      let dt = this._last ? (t - this._last) * 1000 : PAL_FRAME_US;
      this._last = t;
      if (dt > 50000) dt = 50000;     // tab was hidden or machine is slow: don't catch up
      if (dt > 0) this.step(dt);
      this._raf = requestAnimationFrame(this._loop);
    }
    _render() {
      const [x, y, w, h] = this._crop;
      const p = this.w.render(x, y, w, h);
      this._img.data.set(new Uint8ClampedArray(this.w.memory.buffer, p, w * h * 4));
      this._ctx.putImageData(this._img, 0, 0);
      if (this.onframe) this.onframe(this);
    }

    /* ---- readiness (KERNAL booted to READY.) ---- */
    _checkReady() {
      if (this._isReady) return;
      // screen codes for "READY." on the default screen at $0400
      const ram = new Uint8Array(this.w.memory.buffer, this.w.ram_ptr(), 0x10000);
      const s = ram.subarray(0x400, 0x400 + 1000);
      for (let i = 0; i < 994; i++) {
        if (s[i] === 18 && s[i + 1] === 5 && s[i + 2] === 1 && s[i + 3] === 4 && s[i + 4] === 25 && s[i + 5] === 46) {
          this._isReady = true; break;
        }
      }
      if (!this._isReady && this._emuUs > 5e6) this._isReady = true; // give up waiting
      if (this._isReady) { const w = this._readyWaiters; this._readyWaiters = []; w.forEach((f) => f()); }
    }
    get ready() { return this._isReady; }
    whenReady() { return this._isReady ? Promise.resolve() : new Promise((r) => this._readyWaiters.push(r)); }

    /* ---- keyboard ---- */
    /* key(down, code, key): feed a KeyboardEvent's code/key. Returns true if mapped. */
    key(down, code, key) {
      if (!down) {
        const c = this._held.get(code);
        if (c == null) return false;
        this._held.delete(code);
        this.w.key_up(c);
        return true;
      }
      if (this._held.has(code)) return true;    // auto-repeat
      let c = CODE_MAP[code] || 0;
      if (!c && key && key.length === 1) c = charToC64(key, true);
      if (!c) return false;
      this._held.set(code, c);
      this.w.key_down(c);
      return true;
    }
    keyDown(c64code) { this.w.key_down(c64code); }
    keyUp(c64code) { this.w.key_up(c64code); }
    releaseAllKeys() { for (const c of this._held.values()) this.w.key_up(c); this._held.clear(); }
    attachKeyboard(target = global) {
      this.detachKeyboard();
      this._kbdTarget = target;
      target.addEventListener('keydown', this._onKeyDown);
      target.addEventListener('keyup', this._onKeyUp);
      if (global.addEventListener) global.addEventListener('blur', this._blur = () => this.releaseAllKeys());
    }
    detachKeyboard() {
      if (!this._kbdTarget) return;
      this._kbdTarget.removeEventListener('keydown', this._onKeyDown);
      this._kbdTarget.removeEventListener('keyup', this._onKeyUp);
      if (this._blur) global.removeEventListener('blur', this._blur);
      this.releaseAllKeys(); this._kbdTarget = null;
    }
    /* typeText(str): types via the keyboard matrix (40 ms down, 40 ms up per key).
       Letters are typed unshifted (upper case on the C64). '\n' = RETURN.
       Resolves when all keys have been typed. Waits for READY. first. */
    typeText(str) {
      for (const ch of String(str)) {
        const c = charToC64(ch, false);
        if (c) this._typeQueue.push(c);
      }
      return new Promise((r) => this._typeWaiters.push({ r, n: this._typeQueue.length }));
    }
    _typeTick(us) {
      if (!this._isReady) return;
      if (this._typeState === 1 && this._typeT <= 0) {          // key held long enough
        this.w.key_up(this._typeQueue.shift()); this._typeState = 2; this._typeT = TYPE_UP_US;
      } else if (this._typeState === 2 && this._typeT <= 0) {   // gap long enough
        this._typeState = 0;
      }
      if (this._typeState === 0) {
        if (this._typeQueue.length) { this.w.key_down(this._typeQueue[0]); this._typeState = 1; this._typeT = TYPE_DOWN_US; }
        else this._flushTypeWaiters();
      }
      this._typeT -= us;
    }
    _flushTypeWaiters() {
      if (this._typeQueue.length === 0) { const w = this._typeWaiters; this._typeWaiters = []; w.forEach((x) => x.r()); }
    }

    /* ---- joystick: mask of C64.JOY bits, port 1 or 2 ---- */
    joystick(mask, port = 2) {
      this._joy[port === 1 ? 0 : 1] = mask & 31;
      this.w.joystick(this._joy[0], this._joy[1]);
    }

    /* ---- programs ---- */
    /* loadPrg(bytes): copies a .PRG (2-byte load address + data) into RAM and
       sets the BASIC end-of-program pointers. Returns {start, end}. */
    async loadPrg(bytes, { run = false } = {}) {
      bytes = await fetchBytes(bytes);
      if (bytes.length < 3 || bytes.length > this.w.io_size()) throw new Error('c64: bad PRG size');
      await this.whenReady();
      new Uint8Array(this.w.memory.buffer).set(bytes, this.w.io_ptr());
      this.w.quickload(bytes.length);
      const start = bytes[0] | (bytes[1] << 8);
      if (run) await this.run();
      return { start, end: start + bytes.length - 2 };
    }
    run() { return this.typeText('RUN\n'); }
    sys(addr) { return this.typeText('SYS' + (addr | 0) + '\n'); }

    /* ---- memory helpers (RAM view; I/O registers are not visible here) ---- */
    peek(addr) { return this.w.peek(addr & 0xFFFF); }
    poke(addr, v) { this.w.poke(addr & 0xFFFF, v & 0xFF); }
    get ram() { return new Uint8Array(this.w.memory.buffer, this.w.ram_ptr(), 0x10000); }
    /* returns the 40x25 text screen at $0400 as an array of strings */
    readScreen(addr = 0x400) {
      const ram = this.ram, lines = [];
      for (let r = 0; r < 25; r++) {
        let s = '';
        for (let c = 0; c < 40; c++) {
          const v = ram[addr + r * 40 + c] & 0x7F;
          s += v === 0 ? '@' : v < 27 ? String.fromCharCode(64 + v) : v < 32 ? '[\\]^_'[v - 27] : v < 64 ? String.fromCharCode(v) : '#';
        }
        lines.push(s);
      }
      return lines;
    }

    /* ---- audio (SID) ---- */
    get muted() { return this._muted; }
    set muted(m) {
      this._muted = !!m;
      if (!this._muted) this._ensureAudio();
      if (this._audio) {
        this._audio.gain.gain.value = this._muted ? 0 : this._volume;
        if (!this._muted && this._audio.ctx.state === 'suspended') this._audio.ctx.resume();
      }
    }
    get volume() { return this._volume; }
    set volume(v) {
      this._volume = Math.max(0, Math.min(1, +v || 0));
      if (this._audio && !this._muted) this._audio.gain.gain.value = this._volume;
    }
    _ensureAudio() {
      if (this._audio || this._audioPending) return;
      const AC = global.AudioContext || global.webkitAudioContext;
      if (!AC) return;
      let ctx;
      try { ctx = new AC({ sampleRate: SAMPLE_RATE }); } catch (e) { ctx = new AC(); }
      const gain = ctx.createGain();
      gain.gain.value = this._muted ? 0 : this._volume;
      gain.connect(ctx.destination);
      const a = { ctx, gain, node: null, send: null, ratio: ctx.sampleRate / SAMPLE_RATE };
      const useScriptProcessor = () => {
        const q = []; let cur = null, pos = 0, len = 0;
        const sp = ctx.createScriptProcessor(1024, 0, 1);
        sp.onaudioprocess = (e) => {
          const out = e.outputBuffer.getChannelData(0);
          for (let i = 0; i < out.length; i++) {
            if (!cur || pos >= cur.length) { cur = q.shift() || null; pos = 0; if (!cur) { out.fill(0, i); return; } len -= cur.length; }
            out[i] = cur[pos++];
          }
        };
        sp.connect(gain);
        a.node = sp;
        a.send = (buf) => { q.push(buf); len += buf.length; while (len > ctx.sampleRate / 4 && q.length > 1) len -= q.shift().length; };
      };
      this._audioPending = true;
      const done = () => { this._audio = a; this._audioPending = false; this._audioRead = this.w.audio_written(); };
      if (ctx.audioWorklet && global.Blob && global.URL) {
        const url = URL.createObjectURL(new Blob([WORKLET_SRC], { type: 'application/javascript' }));
        ctx.audioWorklet.addModule(url).then(() => {
          const node = new AudioWorkletNode(ctx, 'c64-out', { numberOfInputs: 0, outputChannelCount: [1] });
          node.connect(gain);
          a.node = node; a.send = (buf) => node.port.postMessage(buf, [buf.buffer]);
          done();
        }).catch(() => { useScriptProcessor(); done(); });
      } else { useScriptProcessor(); done(); }
      if (ctx.state === 'suspended') ctx.resume().catch(() => {});
    }
    _pumpAudio() {
      const wr = this.w.audio_written() >>> 0;
      let n = (wr - this._audioRead) >>> 0;
      if (!this._audio || this._muted || !this._audio.send) { this._audioRead = wr; return; }
      const size = this.w.audio_ring_size();
      if (n > size) { this._audioRead = (wr - size) >>> 0; n = size; }
      if (n === 0) return;
      const ring = new Float32Array(this.w.memory.buffer, this.w.audio_ptr(), size);
      let src = new Float32Array(n);
      for (let i = 0; i < n; i++) src[i] = ring[(this._audioRead + i) & (size - 1)];
      this._audioRead = wr;
      const r = this._audio.ratio;
      if (r !== 1) {                       // simple linear resample if the context refused 44.1 kHz
        const m = Math.floor(n * r), dst = new Float32Array(m);
        for (let i = 0; i < m; i++) { const f = i / r, j = f | 0, t = f - j; dst[i] = src[j] * (1 - t) + (src[j + 1 < n ? j + 1 : j]) * t; }
        src = dst;
      }
      this._audio.send(src);
    }

    destroy() {
      this.stop(); this.detachKeyboard();
      if (this._audio) { try { this._audio.ctx.close(); } catch (e) {} this._audio = null; }
    }
  }

  C64.KEY = K;
  C64.JOY = JOY;
  C64.CROPS = CROPS;
  global.C64 = C64;
  if (typeof module === 'object' && module.exports) module.exports = C64;
})(typeof globalThis !== 'undefined' ? globalThis : window);
