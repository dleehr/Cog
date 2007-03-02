//
//  MusepackCodec.m
//  MusepackCodec
//
//  Created by Vincent Spader on 2/21/07.
//  Copyright 2007 __MyCompanyName__. All rights reserved.
//

#import "MonkeysAudioPlugin.h"
#import "MonkeysAudioDecoder.h"
#import "MonkeysAudioPropertiesReader.h"

@implementation MonkeysAudioPlugin

+ (NSDictionary *)pluginInfo
{
	return [NSDictionary dictionaryWithObjectsAndKeys:
		kCogDecoder, [MonkeysAudioDecoder className],
		kCogPropertiesReader, [MonkeysAudioPropertiesReader className],
		nil
	];
}


@end