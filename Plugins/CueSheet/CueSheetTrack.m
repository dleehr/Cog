//
//  CueSheetTrack.m
//  CueSheet
//
//  Created by Zaphod Beeblebrox on 10/8/07.
//  Copyright 2007 __MyCompanyName__. All rights reserved.
//

#import "CueSheetTrack.h"


@implementation CueSheetTrack

+ (id)trackWithURL:(NSURL *)u track:(NSString *)t time:(double)s
{
	return [[[CueSheetTrack alloc] initWithURL:u track:t time:s] autorelease];
}

- (id)initWithURL:(NSURL *)u track:(NSString *)t time:(double)s
{
	self = [super init];
	if (self)
	{
		track = [t copy];
		url = [u copy];
		time = s;
	}
	
	return self;
}

- (NSString *)track
{
	return track;
}

- (NSURL *)url
{
	return url;
}

- (double)time
{
	return time;
}

@end
