use image::{io::Reader as ImageReader, DynamicImage};
use std::{env, io::Read};

fn main() -> Result<(), std::io::Error> {
    let args: Vec<String> = env::args().collect();
    if args.len() < 2 {
        println!("needs an image to work");
        return Ok(());
    }
    let img = ImageReader::open(&args[1])?
        .with_guessed_format()
        .unwrap()
        .decode()
        .unwrap();
    // println!("{:?}", img);
    if let DynamicImage::ImageLuma8(img) = img {
        println!(
            "image {}x{} => {}?{} ",
            img.width(),
            img.height(),
            img.width() * img.height(),
            img.bytes().count()
        );
        assert!(img.width() % 8 == 0);
        assert!(img.height() % 8 == 0);
        println!("constexpr uint8_t IMAGE_WIDTH = {};", img.width());
        println!("constexpr uint8_t IMAGE_HEIGHT = {};", img.height());
        println!("constexpr uint8_t PROGMEM IMAGE_DATA[] =");
        print!("{{ ");
        // { 0b00000000, 0b11000000,
        //   0b01110000, 0b01110000,
        //   0b00000000, 0b00110000 };
        for (i, b) in img.bytes().enumerate() {
            if i % 8 == 0 {
                print!("0b");
            }
            print!("{}", if b.unwrap() == 255 { "0" } else { "1" });
            if (i + 1) % 8 == 0 {
                print!(",  ")
            }
            if (i + 1) % (img.width() as usize) == 0 {
                print!("\n  ");
            }
        }
        println!("}};");
    } else {
        println!("only accepts 8-bit single channel grayscale images");
        return Ok(());
    }
    Ok(())
}
