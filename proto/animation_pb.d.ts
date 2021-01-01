// package: 
// file: animation.proto

import * as jspb from "google-protobuf";
import * as effects_pb from "./effects_pb";

export class TimedAnimationProto extends jspb.Message {
  hasAnimation(): boolean;
  clearAnimation(): void;
  getAnimation(): effects_pb.AnimationProto | undefined;
  setAnimation(value?: effects_pb.AnimationProto): void;

  getTriggerName(): string;
  setTriggerName(value: string): void;

  getStartTimeMsSinceEpoch(): number;
  setStartTimeMsSinceEpoch(value: number): void;

  serializeBinary(): Uint8Array;
  toObject(includeInstance?: boolean): TimedAnimationProto.AsObject;
  static toObject(includeInstance: boolean, msg: TimedAnimationProto): TimedAnimationProto.AsObject;
  static extensions: {[key: number]: jspb.ExtensionFieldInfo<jspb.Message>};
  static extensionsBinary: {[key: number]: jspb.ExtensionFieldBinaryInfo<jspb.Message>};
  static serializeBinaryToWriter(message: TimedAnimationProto, writer: jspb.BinaryWriter): void;
  static deserializeBinary(bytes: Uint8Array): TimedAnimationProto;
  static deserializeBinaryFromReader(message: TimedAnimationProto, reader: jspb.BinaryReader): TimedAnimationProto;
}

export namespace TimedAnimationProto {
  export type AsObject = {
    animation?: effects_pb.AnimationProto.AsObject,
    triggerName: string,
    startTimeMsSinceEpoch: number,
  }
}

