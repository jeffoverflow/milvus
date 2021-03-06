// Copyright (C) 2019-2020 Zilliz. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance
// with the License. You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software distributed under the License
// is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
// or implied. See the License for the specific language governing permissions and limitations under the License.

package mqclient

import (
	"github.com/milvus-io/milvus/internal/util/rocksmq/client/rocksmq"
)

type rmqConsumer struct {
	c          rocksmq.Consumer
	msgChannel chan ConsumerMessage
}

func (rc *rmqConsumer) Subscription() string {
	return rc.c.Subscription()
}

func (rc *rmqConsumer) Chan() <-chan ConsumerMessage {
	return rc.msgChannel
}

func (rc *rmqConsumer) Seek(id MessageID) error {
	msgID := id.(*rmqID).messageID
	return rc.c.Seek(msgID)
}

func (rc *rmqConsumer) Ack(message ConsumerMessage) {
}

func (rc *rmqConsumer) Close() {
}
