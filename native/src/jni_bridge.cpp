#include "lego/color_matcher.hpp"
#include "lego/image_sampler.hpp"
#include "lego/packers.hpp"
#include "lego/renderer.hpp"

#include <jni.h>

#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::string jstringToUtf8(JNIEnv* env, jstring s) {
  if (s == nullptr) {
    return {};
  }
  const char* chars = env->GetStringUTFChars(s, nullptr);
  std::string out = chars ? chars : "";
  env->ReleaseStringUTFChars(s, chars);
  return out;
}

jstring utf8ToJstring(JNIEnv* env, const std::string& s) {
  return env->NewStringUTF(s.c_str());
}

void throwJava(JNIEnv* env, const char* clazz, const std::string& msg) {
  jclass c = env->FindClass(clazz);
  if (c != nullptr) {
    env->ThrowNew(c, msg.c_str());
  }
}

jintArray toJintArray(JNIEnv* env, const std::vector<int>& v) {
  jintArray arr = env->NewIntArray(static_cast<jsize>(v.size()));
  if (arr != nullptr && !v.empty()) {
    env->SetIntArrayRegion(arr, 0, static_cast<jsize>(v.size()),
                           reinterpret_cast<const jint*>(v.data()));
  }
  return arr;
}

std::vector<int> fromJintArray(JNIEnv* env, jintArray arr) {
  if (arr == nullptr) {
    return {};
  }
  jsize n = env->GetArrayLength(arr);
  std::vector<int> out(n);
  if (n > 0) {
    env->GetIntArrayRegion(arr, 0, n, reinterpret_cast<jint*>(out.data()));
  }
  return out;
}

lego::StudGrid studsFromJava(JNIEnv* env, jint w, jint h, jintArray colorIds,
                             jobjectArray colorNames, jbooleanArray present) {
  auto ids = fromJintArray(env, colorIds);
  jsize n = env->GetArrayLength(present);
  std::vector<jboolean> pres(n);
  if (n > 0) {
    env->GetBooleanArrayRegion(present, 0, n, pres.data());
  }
  lego::StudGrid grid(h, std::vector<lego::LegoElement>(w));
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      int i = y * w + x;
      lego::LegoElement el;
      el.present = i < n && pres[i];
      if (el.present) {
        el.colorId = ids[i];
        jstring js = (jstring)env->GetObjectArrayElement(colorNames, i);
        el.colorName = jstringToUtf8(env, js);
        if (js) {
          env->DeleteLocalRef(js);
        }
      }
      grid[y][x] = std::move(el);
    }
  }
  return grid;
}

lego::PlateCatalog catalogFromJava(JNIEnv* env, jintArray colorKeys,
                                   jobjectArray partNums, jintArray widths,
                                   jintArray heights, jintArray counts) {
  auto keys = fromJintArray(env, colorKeys);
  auto ws = fromJintArray(env, widths);
  auto hs = fromJintArray(env, heights);
  auto cnt = fromJintArray(env, counts);
  lego::PlateCatalog cat;
  int cursor = 0;
  for (size_t c = 0; c < keys.size(); c++) {
    int n = cnt[c];
    std::vector<lego::PlateSize> fp;
    fp.reserve(n);
    for (int i = 0; i < n; i++) {
      jstring js = (jstring)env->GetObjectArrayElement(partNums, cursor);
      lego::PlateSize sz;
      sz.partNum = jstringToUtf8(env, js);
      sz.w = ws[cursor];
      sz.h = hs[cursor];
      fp.push_back(std::move(sz));
      if (js) {
        env->DeleteLocalRef(js);
      }
      cursor++;
    }
    cat.byColor[keys[c]] = std::move(fp);
  }
  return cat;
}

jobject packResultToJava(JNIEnv* env, const lego::PackResult& result) {
  jclass partCls = env->FindClass("com/legopicturegenerator/core/pack/PlacedPart");
  jmethodID partCtor =
      env->GetMethodID(partCls, "<init>", "(Ljava/lang/String;ILjava/lang/String;IIII)V");
  jobjectArray placed = env->NewObjectArray(static_cast<jsize>(result.placed.size()),
                                            partCls, nullptr);
  for (jsize i = 0; i < static_cast<jsize>(result.placed.size()); i++) {
    const auto& p = result.placed[i];
    jobject obj = env->NewObject(partCls, partCtor, utf8ToJstring(env, p.partNum),
                                 static_cast<jint>(p.colorId),
                                 utf8ToJstring(env, p.colorName), static_cast<jint>(p.x),
                                 static_cast<jint>(p.y), static_cast<jint>(p.w),
                                 static_cast<jint>(p.h));
    env->SetObjectArrayElement(placed, i, obj);
    env->DeleteLocalRef(obj);
  }

  jclass listCls = env->FindClass("java/util/Arrays");
  jmethodID asList = env->GetStaticMethodID(listCls, "asList",
                                            "([Ljava/lang/Object;)Ljava/util/List;");
  jobject list = env->CallStaticObjectMethod(listCls, asList, placed);

  jclass resultCls = env->FindClass("com/legopicturegenerator/core/pack/PackResult");
  jmethodID resultCtor = env->GetMethodID(
      resultCls, "<init>", "(Ljava/lang/String;Ljava/util/List;JLjava/lang/String;)V");
  return env->NewObject(resultCls, resultCtor, utf8ToJstring(env, result.modeName), list,
                        static_cast<jlong>(result.elapsedMs),
                        utf8ToJstring(env, result.status));
}

}  // namespace

extern "C" {

JNIEXPORT jint JNICALL Java_com_legopicturegenerator_core_nativeengine_NativeEngine_studCountFor(
    JNIEnv*, jclass, jint srcWidth, jint srcHeight, jint blockSize) {
  try {
    return lego::studCountFor(srcWidth, srcHeight, blockSize);
  } catch (const std::exception& e) {
    return 0;
  }
}

JNIEXPORT jint JNICALL
Java_com_legopicturegenerator_core_nativeengine_NativeEngine_blockSizeForTargetStuds(
    JNIEnv* env, jclass, jint srcWidth, jint srcHeight, jint targetStuds) {
  try {
    return lego::blockSizeForTargetStuds(srcWidth, srcHeight, targetStuds);
  } catch (const std::exception& e) {
    throwJava(env, "java/lang/IllegalArgumentException", e.what());
    return 0;
  }
}

JNIEXPORT jintArray JNICALL
Java_com_legopicturegenerator_core_nativeengine_NativeEngine_sampleByBlockSize(
    JNIEnv* env, jclass, jintArray src, jint sw, jint sh, jint blockSize, jintArray outWH) {
  try {
    auto pixels = fromJintArray(env, src);
    auto img = lego::toStudGridByBlockSize(pixels.data(), sw, sh, blockSize);
    jint wh[2] = {img.width, img.height};
    env->SetIntArrayRegion(outWH, 0, 2, wh);
    return toJintArray(env, img.argb);
  } catch (const std::exception& e) {
    throwJava(env, "java/lang/IllegalArgumentException", e.what());
    return nullptr;
  }
}

JNIEXPORT jintArray JNICALL
Java_com_legopicturegenerator_core_nativeengine_NativeEngine_sampleByWidth(
    JNIEnv* env, jclass, jintArray src, jint sw, jint sh, jint targetW, jintArray outWH) {
  try {
    auto pixels = fromJintArray(env, src);
    auto img = lego::toStudGrid(pixels.data(), sw, sh, targetW);
    jint wh[2] = {img.width, img.height};
    env->SetIntArrayRegion(outWH, 0, 2, wh);
    return toJintArray(env, img.argb);
  } catch (const std::exception& e) {
    throwJava(env, "java/lang/IllegalArgumentException", e.what());
    return nullptr;
  }
}

JNIEXPORT jintArray JNICALL
Java_com_legopicturegenerator_core_nativeengine_NativeEngine_matchIndices(
    JNIEnv* env, jclass, jintArray grid, jint width, jint height, jintArray palR,
    jintArray palG, jintArray palB, jintArray outMatched) {
  try {
    auto pixels = fromJintArray(env, grid);
    auto r = fromJintArray(env, palR);
    auto g = fromJintArray(env, palG);
    auto b = fromJintArray(env, palB);
    std::vector<lego::PaletteEntry> pal(r.size());
    for (size_t i = 0; i < r.size(); i++) {
      pal[i] = {r[i], g[i], b[i]};
    }
    auto matched = lego::matchImage(pixels.data(), width, height, pal);
    env->SetIntArrayRegion(outMatched, 0, static_cast<jsize>(matched.matchedArgb.size()),
                           reinterpret_cast<const jint*>(matched.matchedArgb.data()));
    return toJintArray(env, matched.paletteIndex);
  } catch (const std::exception& e) {
    throwJava(env, "java/lang/IllegalArgumentException", e.what());
    return nullptr;
  }
}

JNIEXPORT jobject JNICALL Java_com_legopicturegenerator_core_nativeengine_NativeEngine_pack(
    JNIEnv* env, jclass, jstring mode, jint w, jint h, jintArray colorIds,
    jobjectArray colorNames, jbooleanArray present, jintArray colorKeys,
    jobjectArray partNums, jintArray widths, jintArray heights, jintArray counts) {
  try {
    auto studs = studsFromJava(env, w, h, colorIds, colorNames, present);
    auto cat = catalogFromJava(env, colorKeys, partNums, widths, heights, counts);
    auto result = lego::pack(jstringToUtf8(env, mode), cat, studs);
    return packResultToJava(env, result);
  } catch (const std::exception& e) {
    throwJava(env, "java/lang/IllegalArgumentException", e.what());
    return nullptr;
  }
}

JNIEXPORT jintArray JNICALL
Java_com_legopicturegenerator_core_nativeengine_NativeEngine_renderStuds(
    JNIEnv* env, jclass, jintArray grid, jint cols, jint rows, jint studPx) {
  try {
    auto pixels = fromJintArray(env, grid);
    auto out = lego::renderStuds(pixels.data(), cols, rows, studPx);
    return toJintArray(env, out);
  } catch (const std::exception& e) {
    throwJava(env, "java/lang/IllegalArgumentException", e.what());
    return nullptr;
  }
}

JNIEXPORT jintArray JNICALL
Java_com_legopicturegenerator_core_nativeengine_NativeEngine_renderPacked(
    JNIEnv* env, jclass, jintArray grid, jint cols, jint rows, jint studPx, jintArray xs,
    jintArray ys, jintArray ws, jintArray hs) {
  try {
    auto pixels = fromJintArray(env, grid);
    auto vx = fromJintArray(env, xs);
    auto vy = fromJintArray(env, ys);
    auto vw = fromJintArray(env, ws);
    auto vh = fromJintArray(env, hs);
    std::vector<lego::PlacedPart> placed(vx.size());
    for (size_t i = 0; i < vx.size(); i++) {
      placed[i].x = vx[i];
      placed[i].y = vy[i];
      placed[i].w = vw[i];
      placed[i].h = vh[i];
    }
    auto out = lego::renderPacked(pixels.data(), cols, rows, studPx, placed);
    return toJintArray(env, out);
  } catch (const std::exception& e) {
    throwJava(env, "java/lang/IllegalArgumentException", e.what());
    return nullptr;
  }
}

}  // extern "C"
