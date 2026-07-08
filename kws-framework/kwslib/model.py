"""DS-CNN model: conv -> [depthwise -> pointwise] -> global-avgpool -> dense.

Assembled from kwslib.layers with float forward/backward for training. The trained
float parameters are handed to kwslib.quantize to produce the int8 model that both the
Python int8 reference (infer_int8) and the C engine (kws_model.c) execute.
"""
from __future__ import annotations

import numpy as np

from . import layers


class DSCNN:
    def __init__(self, in_h, in_w, conv_oc=4, conv_k=3, n_blocks=1,
                 pw_oc=4, num_classes=3, seed=0):
        rng = np.random.default_rng(seed)
        self.cfg = dict(in_h=in_h, in_w=in_w, conv_oc=conv_oc, conv_k=conv_k,
                        n_blocks=n_blocks, pw_oc=pw_oc, num_classes=num_classes)
        self.pad = conv_k // 2

        def he(shape, fan_in):
            return rng.standard_normal(shape) * np.sqrt(2.0 / fan_in)

        gap_c = pw_oc if n_blocks else conv_oc
        self.p = {
            'conv_w': he((conv_oc, conv_k, conv_k, 1), conv_k * conv_k),
            'conv_b': np.zeros(conv_oc),
            'fc_w': he((num_classes, gap_c), gap_c),
            'fc_b': np.zeros(num_classes),
        }
        if n_blocks:
            self.p['dw_w'] = he((3, 3, conv_oc), 9)
            self.p['dw_b'] = np.zeros(conv_oc)
            self.p['pw_w'] = he((pw_oc, 1, 1, conv_oc), conv_oc)
            self.p['pw_b'] = np.zeros(pw_oc)

    def forward(self, x):
        c = self.cfg
        cache = {}
        h, cache['conv'] = layers.conv2d_forward(x, self.p['conv_w'], self.p['conv_b'],
                                                 stride=1, pad=self.pad)
        h, cache['conv_relu'] = layers.relu_forward(h)
        if c['n_blocks']:
            h, cache['dw'] = layers.depthwise_forward(h, self.p['dw_w'], self.p['dw_b'],
                                                      stride=1, pad=1)
            h, cache['dw_relu'] = layers.relu_forward(h)
            h, cache['pw'] = layers.conv2d_forward(h, self.p['pw_w'], self.p['pw_b'],
                                                   stride=1, pad=0)
            h, cache['pw_relu'] = layers.relu_forward(h)
        h, cache['gap'] = layers.global_avgpool_forward(h)
        logits, cache['fc'] = layers.dense_forward(h, self.p['fc_w'], self.p['fc_b'])
        return logits, cache

    def backward(self, dlogits, cache):
        c = self.cfg
        g = {}
        dh, g['fc_w'], g['fc_b'] = layers.dense_backward(dlogits, cache['fc'])
        dh = layers.global_avgpool_backward(dh, cache['gap'])
        if c['n_blocks']:
            dh = layers.relu_backward(dh, cache['pw_relu'])
            dh, g['pw_w'], g['pw_b'] = layers.conv2d_backward(dh, cache['pw'])
            dh = layers.relu_backward(dh, cache['dw_relu'])
            dh, g['dw_w'], g['dw_b'] = layers.depthwise_backward(dh, cache['dw'])
        dh = layers.relu_backward(dh, cache['conv_relu'])
        _, g['conv_w'], g['conv_b'] = layers.conv2d_backward(dh, cache['conv'])
        return g

    def loss_and_grads(self, x, labels):
        logits, cache = self.forward(x)
        loss, dlogits, probs = layers.softmax_xent(logits, labels)
        grads = self.backward(dlogits, cache)
        acc = (probs.argmax(axis=1) == labels).mean()
        return loss, grads, acc

    def predict(self, x):
        logits, _ = self.forward(x)
        return logits.argmax(axis=1)
