/*
* Copyright (c) 2024 Huawei Device Co., Ltd.
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/
/**
 * @file
 * @kit ArkGraphics2D
 */
/**
 * @namespace uiEffect
 * @syscap SystemCapability.Graphics.Drawing
 * @since 12
 */
declare namespace uiEffect {
    /**
     * The Filter for Component.
     * @typedef Filter
     * @syscap SystemCapability.Graphics.Drawing
     * @since 12
     */
    interface Filter {
        /**
         * Set blur effect of the Component.
         *
         * @param { number } blurRadius
         * @returns { Filter }
         * @syscap SystemCapability.Graphics.Drawing
         * @since 12
         */
        blur(blurRadius: number): Filter;
        /**
         * Detects and glows edges of contents.
         *
         * @param { number } alpha
         * @param { Color } [color]
         * @param { Object } [mask]
         * @param { boolean } [bloom]
         * @returns { Filter } - Returns the Filter that the current effect have been added.
         * @syscap SystemCapability.Graphics.Drawing
         * @systemapi
         * @since 20
         */
        edgeLight(alpha: number, color?: Color, mask?: Object, bloom?: boolean): Filter;
    }
    /**
     * The VisualEffect of Component.
     * @typedef VisualEffect
     * @syscap SystemCapability.Graphics.Drawing
     * @since 12
     */
    interface VisualEffect {
    }
    /**
     * Create a Filter to add multiple effects to the component.
     * @returns { Filter } Returns the head node of Filter.
     * @syscap SystemCapability.Graphics.Drawing
     * @since 12
     */
    function createFilter(): Filter;
    /**
     * Apply a registered AuroraGraph custom effect by reverse-DNS name.
     * AuroraGraph SDK patch (stand-in typing until T9 codegen).
     *
     * @param { string } name - effect name, e.g. 'com.aurora.edge_light_red'
     * @param { Record<string, Object> } [params] - optional params record (e.g. { 'alpha': 1.0 })
     * @returns { Filter } - Returns the Filter with the custom effect applied.
     * @syscap SystemCapability.Graphics.Drawing
     * @systemapi
     * @since 12
     */
    function applyCustomEffect(name: string, params?: Record<string, Object>): Filter;
    /**
     * Create a VisualEffect to add multiple effects to the component.
     * @returns { VisualEffect } Returns the head node of visualEffect.
     * @syscap SystemCapability.Graphics.Drawing
     * @since 12
     */
    function createEffect(): VisualEffect;
}
export default uiEffect;
