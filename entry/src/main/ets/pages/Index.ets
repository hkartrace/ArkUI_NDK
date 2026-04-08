import { NodeContent } from '@kit.ArkUI';
import nativeNode from 'libentry.so';

@Entry
@Component
struct Index {
  private rootSlot = new NodeContent();

  build() {
    Column() {
      ContentSlot(this.rootSlot)
    }
    .width('100%')
    .height('100%')
    .onAppear(() => {
      nativeNode.createNativeRoot(this.rootSlot);
    })
  }
}
