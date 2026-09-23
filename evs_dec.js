/* evs_dec.js — PLACEHOLDER
 * Delete me and drop the real emcc output in place once `./build.sh` succeeds.
 * This stub keeps index.html's feature-detect happy but fails cleanly so the
 * page shows "WASM Missing" until the real decoder is present. */
var createEvsDecModule = function (moduleArg) {
  return Promise.reject(new Error(
    "EVS decoder not built yet. Run ./build.sh and replace evs_dec.js + evs_dec.wasm."
  ));
};
if (typeof module !== 'undefined') module.exports = createEvsDecModule;