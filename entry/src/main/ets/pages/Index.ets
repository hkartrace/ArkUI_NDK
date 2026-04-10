import { NodeContent } from '@kit.ArkUI';
import nativeNode from 'libentry.so';

@Entry
@Component
@Preview
struct Index {
  private rootSlot = new NodeContent();

  build() {
    Column() {
      ContentSlot(this.rootSlot)
    }
    .backgroundColor(Color.Green)
    .width('100%')
    .height('100%')
    .onAppear(() => {
      nativeNode.createNativeRoot(this.rootSlot);
    })

  }
}
