import uiEffect from '@ohos.graphics.uiEffect';

@Entry
@Component
struct Index {
  @State message: string = 'AuroraGraph T4';

  build() {
    RelativeContainer() {
      Column({ space: 24 }) {
        Rect({ width: '70%', height: '18%' })
          .radiusWidth(16)
          .radiusHeight(16)
          .fill(Color.White)
        Text(this.message)
          .id('HelloWorld')
          .fontSize($r('app.float.page_text_font_size'))
          .fontWeight(FontWeight.Bold)
          .fontColor(Color.White)
          .onClick(() => {
            this.message = 'Welcome';
          })
      }
      .justifyContent(FlexAlign.Center)
      .height('60%')
      .width('80%')
      .backgroundColor('#101020')
      .borderRadius(24)
      // Run10-T1: apply the GRAPH effect (grey member + passthrough island)
      // generated from effect.graph.json by effect-sdk\graph\graph-codegen.js.
      .foregroundFilter(uiEffect.applyCustomEffect('com.aurora.edge_light_red'))
      .alignRules({
        center: { anchor: '__container__', align: VerticalAlign.Center },
        middle: { anchor: '__container__', align: HorizontalAlign.Center }
      })
    }
    .height('100%')
    .width('100%')
  }
}

