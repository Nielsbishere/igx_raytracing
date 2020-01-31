# Raytracing optimization

## Raygen

Two types of culling can be used that are screen relative; 

### Screen "frustum" culling

Each light or object that falls outside of this area is effectively culled from the screen's light and object buffer (uint with uint[]). BUT they will still be present in the global array, since it is needed for the bounce stage.

### Screen tile culling

Basically the same as screen "frustum" culling but instead of doing it for the visible pixels, you do it for the compute local pixels. This means you have to store 4(x+1)(ceil(w / tw))(ceil(h / th)) bytes worth of data to represent this. For for 1920x1080 at 8x8: `(x+1)*240*135*4`=129'600 bytes. If we wanted to store the maximum amount of objects we need; tw * th. 
8'424'000 bytes; ~8 MiB. This means it will roughly need 4wh+4 bytes, but wh aren't always identical because they are rounded up to the nearest tile size. So for 1080p if you want to be able to store the maximum number of objects that are stored you'd always use around 8 MiB.

## Bounce 

### Acceleration structure